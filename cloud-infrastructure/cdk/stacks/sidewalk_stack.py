# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

from pathlib import Path
import aws_cdk as cdk
from aws_cdk import Duration, aws_iam as iam
from aws_cdk import aws_iot as iot
from aws_cdk import aws_lambda as _lambda
from aws_cdk import aws_logs as logs
from constructs import Construct

from aws_cdk.custom_resources import (
    AwsCustomResource,
    AwsCustomResourcePolicy,
    AwsSdkCall,
    PhysicalResourceId,
)

from .workshop_studio import NestedWorkshopStudioStack


class SidewalkStack(NestedWorkshopStudioStack):
    def __init__(self, scope: Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)

        # This code will deploy lambda for sidewalk payload deserialization and create a IAM role that will be used for Sidewalk destination

        sidewalk_log_group = logs.LogGroup(
            self,
            "sidewalk_rule_logs",
            removal_policy=cdk.RemovalPolicy.DESTROY,
            log_group_name="sidewalk_rule_errors",
            retention=logs.RetentionDays.ONE_WEEK,
        )

        sidewalk_destination_policy = iam.Policy(
            self,
            "Sidewalk Destination Policy",
            policy_name="sidewalk_destination_policy",
            document=iam.PolicyDocument(
                statements=[
                    iam.PolicyStatement(
                        actions=["iot:*"],
                        resources=[
                            f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/*"
                        ],
                        effect=iam.Effect.ALLOW,
                    ),
                    iam.PolicyStatement(
                        actions=["iot:DescribeEndpoint"],
                        resources=[f"*"],
                        effect=iam.Effect.ALLOW,
                    ),
                ]
            ),
        )
        self.sidewalk_destination_role = iam.Role(
            self,
            "SidewalkDestinationRole",
            role_name="DemoBadgeSidewalkDestinationRole",
            assumed_by=iam.ServicePrincipal("iotwireless.amazonaws.com"),
        )
        self.sidewalk_destination_role.attach_inline_policy(sidewalk_destination_policy)

        sidewalk_lambda_policy = iam.Policy(
            self,
            "Sidewalk Lambda Policy",
            policy_name="sidewalk_lambda_policy",
            document=iam.PolicyDocument(
                statements=[
                    iam.PolicyStatement(
                        actions=[
                            "logs:CreateLogGroup",
                            "logs:CreateLogStream",
                            "logs:PutLogEvents",
                        ],
                        resources=[
                            f"arn:{self.partition}:iot:{self.region}:{self.account}:*"
                        ],
                        effect=iam.Effect.ALLOW,
                    ),
                ]
            ),
        )
        sidewalk_lambda_role = iam.Role(
            self,
            "Sidewalk Lambda Role",
            role_name="sidewalk_lambda_role",
            assumed_by=iam.ServicePrincipal("lambda.amazonaws.com"),
        )
        sidewalk_lambda_role.attach_inline_policy(sidewalk_lambda_policy)

        error_action_role = iam.Role(
            self,
            "IoTErrorActionRole",
            role_name="IoTErrorActionRole",
            assumed_by=iam.ServicePrincipal("iot.amazonaws.com"),
        )
        error_action_policy = iam.Policy(
            self,
            "ErrorActionPolicy",
            policy_name="ErrorActionPolicy",
            statements=[
                iam.PolicyStatement(
                    actions=[
                        "logs:CreateLogStream",
                        "logs:DescribeLogStreams",
                        "logs:PutLogEvents",
                    ],
                    resources=[sidewalk_log_group.log_group_arn],
                )
            ],
        )
        error_action_role.attach_inline_policy(error_action_policy)

        republish_action_role = iam.Role(
            self,
            "IoTRepublishActionRole",
            role_name="IoTRepublishActionRole",
            assumed_by=iam.ServicePrincipal("iot.amazonaws.com"),
        )
        republish_policy = iam.Policy(
            self,
            "IoTRepublishActionPolicy",
            policy_name="IoTRepublishActionPolicy",
            statements=[
                iam.PolicyStatement(
                    actions=["iot:Publish"],
                    resources=[
                        f"arn:{self.partition}:iot:{self.region}:{self.account}:topic/*"
                    ],
                )
            ],
        )
        republish_action_role.attach_inline_policy(republish_policy)

        # Additional permissions needed for manual destination creation via console for participants
        additional_sidewalk_policy = iam.Policy(self, "SidewalkAdditionalPolicy")
        p: iam.CfnPolicy = additional_sidewalk_policy.node.default_child
        p.cfn_options.condition = self.is_workshop_studio_condition
        additional_sidewalk_policy.attach_to_role(
            iam.Role.from_role_name(
                self, "ParticipantRole", self.participant_role_name.value_as_string
            )
        )
        additional_sidewalk_policy.add_statements(
            iam.PolicyStatement(
                effect=iam.Effect.ALLOW,
                actions=[
                    "iam:PassRole"
                ],
                resources=[
                    self.sidewalk_destination_role.role_arn,
                ],
            )
        )

        lambda_path = (
            Path(__file__).parents[1].joinpath("lambda", "SidewalkDecoderLambda")
        )
        lambda_code = _lambda.Code.from_asset(
            path=str(lambda_path),
        )

        sidewalk_decoder_lambda = _lambda.Function(
            self,
            "SidewalkDecoderLambda",
            runtime=_lambda.Runtime.PYTHON_3_11,
            code=lambda_code,
            handler="main.lambda_handler",
            role=sidewalk_lambda_role,
            function_name="sidewalk_lambda",
            timeout=Duration.seconds(5),
        )

        # This is needed for IoT Rule to be able to trigger the Lambda function
        sidewalk_decoder_lambda.add_permission(
            id="sidewalk_decoder_lambda_invoke_function",
            principal=iam.ServicePrincipal("iot.amazonaws.com"),
            source_arn=f"arn:{self.partition}:iot:{self.region}:{self.account}:*",
            action="lambda:InvokeFunction",
        )

        # Rule engine and actions definitions
        rule_name = "sidewalk_uplink_rule"
        iot_topic_rule_sql = f'SELECT aws_lambda("{sidewalk_decoder_lambda.function_arn}", {{"raw_payload":PayloadData}}) as data FROM "$aws/rules/{rule_name}"'
        iot_topic_rule = iot.CfnTopicRule(
            self,
            "SidewalkUplinkRule",
            rule_name=rule_name,
            topic_rule_payload=iot.CfnTopicRule.TopicRulePayloadProperty(
                sql=iot_topic_rule_sql,
                aws_iot_sql_version="2016-03-23",
                actions=[
                    iot.CfnTopicRule.ActionProperty(
                        republish=iot.CfnTopicRule.RepublishActionProperty(
                            role_arn=republish_action_role.role_arn,
                            topic="$$aws/rules/demo_badge_sensors",  # republishing to a reserved topic requires double-dollar! https://docs.aws.amazon.com/iot/latest/developerguide/republish-rule-action.html
                            qos=1,
                        )
                    )
                ],
                error_action=iot.CfnTopicRule.ActionProperty(
                    cloudwatch_logs=iot.CfnTopicRule.CloudwatchLogsActionProperty(
                        log_group_name=sidewalk_log_group.log_group_name,
                        role_arn=error_action_role.role_arn,
                    ),
                ),
            ),
        )

        on_create = AwsSdkCall(
            service="IoTWireless",
            action="updateEventConfigurationByResourceTypes",
            parameters={
                'DeviceRegistrationState': {'Sidewalk': {'WirelessDeviceEventTopic': 'Enabled'}},
                'MessageDeliveryStatus': {'Sidewalk': {'WirelessDeviceEventTopic': 'Enabled'}},
                'Proximity': {'Sidewalk': {'WirelessDeviceEventTopic': 'Enabled'}},
            },
            physical_resource_id=PhysicalResourceId.of('IoTWirelessEventConfigurationUpdate')
        )
        AwsCustomResource(
            self,
            "IoTWirelessEventConfigurationUpdate",
            resource_type="Custom::IoTWirelessEventConfigurationUpdate",
            policy=AwsCustomResourcePolicy.from_sdk_calls(resources=["*"]),
            on_create=on_create,
            on_update=on_create,
        )
