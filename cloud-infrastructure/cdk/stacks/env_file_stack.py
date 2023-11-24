# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import json

import aws_cdk as cdk

from aws_cdk.custom_resources import (
    AwsCustomResource,
    AwsCustomResourcePolicy,
    PhysicalResourceId,
    AwsSdkCall,
)

from constructs import Construct

from .workshop_studio import NestedWorkshopStudioStack


class EnvFileStack(NestedWorkshopStudioStack):
    def __init__(
        self,
        scope: Construct,
        construct_id: str,
        sidewalk_provisioning_lambda: cdk.aws_lambda.Function,
        grafana_url: str,
        identity_pool_id: str,
        companion_web_app_bucket: cdk.aws_s3.Bucket,
        companion_web_app_url: str,
        cognito_redirect_url: str,
        cognito_policy_name: str,
        user_pool_id: str,
        ota_role_arn: str,
        timestream_db_name: str,
        timestream_table_name: str,
        timestream_insert_role_arn: str,
        **kwargs,
    ) -> None:
        super().__init__(scope, construct_id, **kwargs)

        env_json_filename = "env.json"
        env_json_physical_resource_id = PhysicalResourceId.of(
            "CustomResourceCompanionWebAppEnvFile"
        )

        create_params = {
            "Bucket": companion_web_app_bucket.bucket_name,
            "Key": env_json_filename,
            "Body": json.dumps(
                {
                    "AWSAccountId": self.account,
                    "AWSRegion": self.region,
                    "AWSCognitoIdentityPoolId": identity_pool_id,
                    "SidewalkProvisioningLambdaFunctionName": sidewalk_provisioning_lambda.function_name,
                    "WiFiSSID": "iot-workshop",
                    "WiFiPassphrase": "AWSomeIOT!",
                    "ImageS3Bucket": companion_web_app_bucket.bucket_name,
                    "CompanionWebAppURL": companion_web_app_url,
                    "GrafanaURL": grafana_url,
                    "OTARoleARN": ota_role_arn,
                    "RuleActionTimestreamRoleARN": timestream_insert_role_arn,
                    "CognitoRedirect": cognito_redirect_url,
                    "CognitoIoTPolicyName": cognito_policy_name,
                    "UserPoolID": user_pool_id,
                    "TimestreamDatabase": timestream_db_name,
                    "TimestreamTable": timestream_table_name,
                }
            ),
        }
        on_create = AwsSdkCall(
            service="S3",
            action="putObject",
            parameters=create_params,
            physical_resource_id=env_json_physical_resource_id,
        )
        on_delete = AwsSdkCall(
            service="S3",
            action="deleteObject",
            parameters={
                "Bucket": companion_web_app_bucket.bucket_name,
                "Key": env_json_filename,
            },
            physical_resource_id=env_json_physical_resource_id,
        )
        env_file = AwsCustomResource(
            self,
            "CompanionWebAppEnvFile",
            resource_type="Custom::CompanionWebAppEnvFile",
            policy=AwsCustomResourcePolicy.from_sdk_calls(
                resources=[
                    companion_web_app_bucket.bucket_arn + "/*",
                ]
            ),
            on_create=on_create,
            on_update=on_create,
            on_delete=on_delete,
        )
