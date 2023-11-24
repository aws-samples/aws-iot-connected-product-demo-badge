# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import os
import textwrap
from pathlib import Path

import aws_cdk as cdk
from aws_cdk import (
    BundlingOptions,
    CfnOutput,
    Duration,
    RemovalPolicy,
    aws_iam,
    aws_iot,
    aws_cloudfront,
    aws_cloudfront_origins,
    aws_lambda,
    aws_s3,
    aws_s3_deployment,
    aws_iot,
    aws_cognito as cognito,
)
import uuid as uuid

from aws_cdk.aws_cognito_identitypool_alpha import (
    IdentityPool,
    UserPoolAuthenticationProvider,
    IdentityPoolAuthenticationProviders,
)

from aws_cdk.custom_resources import (
    AwsCustomResource,
    AwsCustomResourcePolicy,
    AwsSdkCall,
    PhysicalResourceId,
)

from constructs import Construct

from .workshop_studio import NestedWorkshopStudioStack

local_development_server = "127.0.0.1:8887"


class CompanionWebAppStack(NestedWorkshopStudioStack):
    def create_app(self):
        self.companion_web_app_bucket = aws_s3.Bucket(
            self,
            "CompanionWebAppBucket",
            removal_policy=RemovalPolicy.DESTROY,
            auto_delete_objects=True,
            versioned=True,  # needed for AWS IoT Job with OTA file stream from S3 object
            block_public_access=aws_s3.BlockPublicAccess.BLOCK_ALL,
            encryption=aws_s3.BucketEncryption.S3_MANAGED,
            cors=[
                aws_s3.CorsRule(
                    allowed_headers=["*"],
                    allowed_methods=[aws_s3.HttpMethods.PUT],
                    allowed_origins=[
                        "https://*.cloudfront.net",
                        f"http://{local_development_server}",
                        f"https://{local_development_server}",
                    ],
                )
            ],
        )

        cwa_path = Path(__file__).parents[3].joinpath("companion_web_app")
        if "USE_PRE_BUNDLED_ASSETS" in os.environ:
            cwa_path = cwa_path.joinpath("dist")
            cwa_bundling_options = None
        else:
            cwa_bundling_options = BundlingOptions(
                image=aws_lambda.Runtime.NODEJS_18_X.bundling_image,
                command=[
                    "bash",
                    "-c",
                    "export npm_config_cache=/asset-input/tmp/.npm && /asset-input/build.sh && cp -r /asset-input/dist/* /asset-output/",
                ],
            )

        # from https://github.com/aws-samples/amazon-cloudfront-functions/blob/4b5606630db439f3d47c9964547f532646dd2c65/url-rewrite-single-page-apps/index.js
        cloudfront_redirect_function = aws_cloudfront.Function(
            self,
            "IndexHTMLRedirectFunction",
            code=aws_cloudfront.FunctionCode.from_inline(
                textwrap.dedent(
                    """
                function handler(event) {
                    var request = event.request;
                    var uri = request.uri;

                    // Check whether the URI is missing a file name.
                    if (uri.endsWith('/')) {
                        request.uri += 'index.html';
                    }
                    // Check whether the URI is missing a file extension.
                    else if (!uri.includes('.')) {
                        request.uri += '/index.html';
                    }

                    return request;
                }
            """
                )
            ),
        )

        self.distribution = aws_cloudfront.Distribution(
            self,
            "CloudFrontDistribution",
            default_root_object="index.html",
            default_behavior=aws_cloudfront.BehaviorOptions(
                origin=aws_cloudfront_origins.S3Origin(self.companion_web_app_bucket),
                viewer_protocol_policy=aws_cloudfront.ViewerProtocolPolicy.REDIRECT_TO_HTTPS,
                function_associations=[
                    aws_cloudfront.FunctionAssociation(
                        event_type=aws_cloudfront.FunctionEventType.VIEWER_REQUEST,
                        function=cloudfront_redirect_function,
                    )
                ],
            ),
        )

        self.bucket_deployment = aws_s3_deployment.BucketDeployment(
            self,
            "CompanionWebAppAssets",
            sources=[
                aws_s3_deployment.Source.asset(
                    path=str(cwa_path),
                    bundling=cwa_bundling_options,
                )
            ],
            destination_bucket=self.companion_web_app_bucket,
            distribution=self.distribution,
            distribution_paths=["/*"],
            prune=False,  # needed for creation of env.json in a separate step
        )

        self.companion_web_app_url = CfnOutput(
            self, "CompanionWebAppURL", value=f"https://{self.distribution.domain_name}"
        )

    def create_cognito(self, username, password):
        self.user_pool = cognito.UserPool(
            self,
            "CompanionWebAppUserPool",
            user_pool_name="web_app_user_pool",
            self_sign_up_enabled=False,
            removal_policy=cdk.RemovalPolicy.DESTROY,
            password_policy=cognito.PasswordPolicy(
                min_length=6,
                require_digits=False,
                require_lowercase=False,
                require_symbols=False,
                require_uppercase=False,
            ),
        )

        self.app_client = self.user_pool.add_client(
            "CompanionWebAppClient",
            user_pool_client_name="companion_web_app_client",
            access_token_validity=Duration.hours(24),
            id_token_validity=Duration.hours(24),
            refresh_token_validity=Duration.hours(24),
            enable_token_revocation=True,
            auth_flows=cognito.AuthFlow(user_srp=True),
            o_auth=cognito.OAuthSettings(
                callback_urls=[
                    f"https://{self.distribution.domain_name}",
                    f"https://{local_development_server}",  # Cognito only support HTTPS callback URLs
                ],
                flows=cognito.OAuthFlows(implicit_code_grant=True),
            ),
        )
        self.app_client.node.add_dependency(self.user_pool)

        self.user_pool_domain = cognito.UserPoolDomain(
            self,
            "CompanionWebAppCognitoUserPoolDomain",
            user_pool=self.user_pool,
            cognito_domain=cognito.CognitoDomainOptions(
                domain_prefix=f"demo-bage-workshop-{self.account}"
            ),
        )
        self.identity_pool = IdentityPool(
            self,
            "CompanionWebAppIdentityPool",
            allow_unauthenticated_identities=False,
            authentication_providers=IdentityPoolAuthenticationProviders(
                user_pools=[
                    UserPoolAuthenticationProvider(
                        user_pool=self.user_pool, user_pool_client=self.app_client
                    )
                ]
            ),
            identity_pool_name="CompanionWebAppIdentityPool",
        )

        # form hosted UI URL to return back as CFN Output for user login
        self.cognito_app_redirect_url = f"https://{self.user_pool_domain.domain_name}.auth.{self.region}.amazoncognito.com/oauth2/authorize?client_id={self.app_client.user_pool_client_id}&response_type=token&scope=aws.cognito.signin.user.admin+email+openid+phone+profile"

        admin_create_user = AwsCustomResource(
            self,
            "AwsCustomResource-CreateUser",
            resource_type="Custom::AwsCustomResource-CreateUser",
            on_create=AwsSdkCall(
                service="CognitoIdentityServiceProvider",
                action="adminCreateUser",
                parameters={
                    "UserPoolId": self.user_pool.user_pool_id,
                    "Username": username,
                    "MessageAction": "SUPPRESS",
                    "TemporaryPassword": password,
                },
                physical_resource_id=PhysicalResourceId.of(
                    "AwsCustomResource-CreateUser"
                ),
            ),
            policy=AwsCustomResourcePolicy.from_sdk_calls(
                resources=AwsCustomResourcePolicy.ANY_RESOURCE
            ),
            install_latest_aws_sdk=True,
        )
        admin_create_user.node.add_dependency(self.user_pool)

        # attach iot policy for authenticate role - this will be attached to every IdentityID that logs into the the web app
        self.cognito_iot_policy = aws_iot.CfnPolicy(
            self,
            "CognitoIoTPolicy",
            policy_name="CognitoIoTPolicy",
            policy_document={
                "Version": "2012-10-17",
                "Statement": [
                    {
                        "Effect": "Allow",
                        "Action": [
                            "iot:Connect",
                        ],
                        "Resource": ["*"],
                    },
                    {
                        "Effect": "Allow",
                        "Action": [
                            "iot:Publish",
                            "iot:Receive",
                        ],
                        "Resource": [
                            f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/*",
                        ],
                    },
                    {
                        "Effect": "Allow",
                        "Action": [
                            "iot:Subscribe",
                        ],
                        "Resource": [
                            f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/*",
                        ],
                    },
                ],
            },
        )

        admin_set_user_password = AwsCustomResource(
            self,
            "AwsCustomResource-ForcePassword",
            resource_type="Custom::AwsCustomResource-ForcePassword",
            on_create=AwsSdkCall(
                service="CognitoIdentityServiceProvider",
                action="adminSetUserPassword",
                parameters={
                    "UserPoolId": self.user_pool.user_pool_id,
                    "Username": username,
                    "Password": password,
                    "Permanent": True,
                },
                physical_resource_id=PhysicalResourceId.of(
                    "AwsCustomResource-ForcePassword"
                ),
            ),
            policy=AwsCustomResourcePolicy.from_sdk_calls(
                resources=AwsCustomResourcePolicy.ANY_RESOURCE
            ),
            install_latest_aws_sdk=True,
        )
        admin_set_user_password.node.add_dependency(admin_create_user)

    def create_ota_role(self):
        self.ota_role = aws_iam.Role(
            self,
            "OTARole",
            role_name="DemoBadgeOTAJobRole",
            assumed_by=aws_iam.CompositePrincipal(
                aws_iam.ServicePrincipal("iot.amazonaws.com"),
                aws_iam.ArnPrincipal(
                    f"arn:{self.partition}:sts::{self.account}:assumed-role/{self.identity_pool.authenticated_role.role_name}/CognitoIdentityCredentials"
                ),
            ),
        )
        self.ota_role.add_managed_policy(
            aws_iam.ManagedPolicy.from_aws_managed_policy_name(
                "service-role/AmazonFreeRTOSOTAUpdate"
            )
        )
        self.ota_role.add_to_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "s3:GetObjectVersion",
                    "s3:GetObject",
                    "s3:PutObject",
                ],
                resources=[f"{self.companion_web_app_bucket.bucket_arn}/*"],
            )
        )
        self.ota_role.add_to_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "iam:GetRole",
                    "iam:PassRole",
                ],
                resources=[self.ota_role.role_arn],
            )
        )

        additional_ota_policy = aws_iam.Policy(
            self, "OTAAdditionalPassRoleWSParticipantPolicy"
        )
        p: aws_iam.CfnPolicy = additional_ota_policy.node.default_child
        p.cfn_options.condition = self.is_workshop_studio_condition
        additional_ota_policy.attach_to_role(
            aws_iam.Role.from_role_name(
                self, "ParticipantRole", self.participant_role_name.value_as_string
            )
        )
        additional_ota_policy.add_statements(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "iam:PassRole",
                ],
                resources=[self.ota_role.role_arn],
            )
        )

    def set_cognito_authenticated_policy(
        self,
        sidewalk_provisioning_lambda,
        sidewalk_destination_role_arn,
        timestream_insert_role_arn,
    ):
        self.identity_pool.authenticated_role.add_to_principal_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=["iam:AssumeRole"],
                resources=[self.ota_role.role_arn],
            )
        )
        self.identity_pool.authenticated_role.add_to_principal_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=["s3:PutObject"],
                resources=[f"{self.companion_web_app_bucket.bucket_arn}/*"],
            )
        )
        self.identity_pool.authenticated_role.add_to_principal_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=["lambda:InvokeFunction"],
                resources=[sidewalk_provisioning_lambda.function_arn],
            )
        )
        self.identity_pool.authenticated_role.add_to_principal_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "iot:DeleteTopicRule",
                ],
                resources=[
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:rule/demo_badge_sensors",
                ],
            )
        )
        self.identity_pool.authenticated_role.add_to_principal_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "iot:Connect",
                    "iot:GetThingShadow",
                    "iot:UpdateThingShadow",
                    "iot:AttachPolicy",
                    "iot:AttachPrincipalPolicy",
                    "iot:AttachThingPrincipal",
                    "iot:CreateThing",
                    "iot:DeleteThing",
                    "iot:DeleteCertificate",
                    "iot:RegisterCertificateWithoutCA",
                    "iot:CreateTopicRule",
                    "iotwireless:SendDataToWirelessDevice",
                    "iotwireless:GetDestination",
                    "iotwireless:GetDeviceProfile",
                    "iotwireless:ListDestinations",
                    "iotwireless:ListDeviceProfiles",
                    "iotwireless:ListWirelessDevices",
                    "iotwireless:CreateDestination",
                    "iotwireless:CreateDeviceProfile",
                    "iotwireless:CreateWirelessDevice",
                    "iotwireless:DeleteDestination",
                    "iotwireless:DeleteDeviceProfile",
                    "iotwireless:DeleteWirelessDevice",
                    "iotwireless:DeregisterWirelessDevice",
                ],
                resources=["*"],
            )
        )
        self.identity_pool.authenticated_role.add_to_principal_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "iot:Publish",
                    "iot:Receive",
                ],
                resources=[
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/$aws/things/*/shadow",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/$aws/things/*/shadow/*",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/hello/badge",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/hello/cloud",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/hello/world",
                ],
            )
        )
        self.identity_pool.authenticated_role.add_to_principal_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "iot:Receive",
                ],
                resources=[
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/$aws/events/presence/connected/*",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/$aws/events/presence/disconnected/*",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/$aws/device_location/*/get_position_estimate/accepted",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/$aws/device_location/*/get_position_estimate/rejected",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/demo_badge/sensors",
                ],
            )
        )
        self.identity_pool.authenticated_role.add_to_principal_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "iot:Subscribe",
                ],
                resources=[
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/$aws/events/presence/connected/*",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/$aws/events/presence/disconnected/*",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/$aws/things/*/shadow",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/$aws/things/*/shadow/*",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/$aws/device_location/*/get_position_estimate/accepted",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/$aws/device_location/*/get_position_estimate/rejected",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/demo_badge/sensors",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/hello/badge",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/hello/cloud",
                    f"arn:{self.partition}:iot:{self.region}:{self.account}:topicfilter/hello/world",
                ],
            )
        )
        self.identity_pool.authenticated_role.add_to_principal_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "iot:DescribeEndpoint",
                    "iot:ListThings",
                ],
                resources=["*"],
            )
        )
        self.identity_pool.authenticated_role.add_managed_policy(
            aws_iam.ManagedPolicy.from_aws_managed_policy_name(
                "service-role/AmazonFreeRTOSOTAUpdate"
            )
        )
        self.identity_pool.authenticated_role.add_to_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "s3:GetObjectVersion",
                    "s3:GetObject",
                    "s3:PutObject",
                ],
                resources=[f"{self.companion_web_app_bucket.bucket_arn}/*"],
            )
        )
        self.identity_pool.authenticated_role.add_to_policy(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "iam:GetRole",
                    "iam:PassRole",
                ],
                resources=[
                    self.ota_role.role_arn,
                    sidewalk_destination_role_arn,
                    timestream_insert_role_arn,
                ],
            )
        )

    def cognito_hosted_ui_customization(self):
        set_ui_customization_lambda_role = aws_iam.Role(
            self,
            "SetUICustomizationLambdaRole",
            assumed_by=aws_iam.ServicePrincipal("lambda.amazonaws.com"),
            inline_policies={
                "IoTWireless": aws_iam.PolicyDocument(
                    statements=[
                        aws_iam.PolicyStatement(
                            effect=aws_iam.Effect.ALLOW,
                            actions=["cognito-idp:SetUICustomization"],
                            resources=[self.user_pool.user_pool_arn],
                        ),
                        aws_iam.PolicyStatement(
                            effect=aws_iam.Effect.ALLOW,
                            actions=[
                                "logs:CreateLogGroup",
                                "logs:CreateLogStream",
                                "logs:PutLogEvents",
                            ],
                            resources=["*"],
                        ),
                    ]
                )
            },
        )
        set_ui_customization_function = aws_lambda.Function(
            self,
            "CustomResourceOnEventHandlerFunction",
            runtime=aws_lambda.Runtime.PYTHON_3_11,
            handler="main.lambda_handler",
            role=set_ui_customization_lambda_role,
            timeout=Duration.seconds(30),
            code=aws_lambda.Code.from_asset(
                str(
                    Path(__file__)
                    .parents[1]
                    .joinpath("lambda", "CognitoHostedUICustomization")
                )
            ),
        )
        set_ui_customization_custom_resource = cdk.CustomResource(
            self,
            "SetUICustomizationCustomResource",
            service_token=set_ui_customization_function.function_arn,
            properties={
                "UserPoolId": self.user_pool.user_pool_id,
                "ClientId": "ALL",
            },
        )
        set_ui_customization_custom_resource.node.add_dependency(self.identity_pool)
        set_ui_customization_custom_resource.node.add_dependency(self.user_pool)
        set_ui_customization_custom_resource.node.add_dependency(self.user_pool_domain)
        set_ui_customization_custom_resource.node.add_dependency(self.app_client)

    def __init__(
        self,
        scope: Construct,
        construct_id: str,
        sidewalk_provisioning_lambda,
        sidewalk_destination_role_arn: str,
        timestream_insert_role_arn: str,
        username: str,
        password: str,
        **kwargs,
    ) -> None:
        super().__init__(scope, construct_id, **kwargs)

        self.create_app()

        self.create_cognito(username, password)

        self.create_ota_role()

        self.set_cognito_authenticated_policy(
            sidewalk_provisioning_lambda,
            sidewalk_destination_role_arn,
            timestream_insert_role_arn,
        )

        self.cognito_hosted_ui_customization()
