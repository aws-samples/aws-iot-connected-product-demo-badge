# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

from constructs import Construct

from .companion_web_app_stack import *
from .credentials_stack import *
from .grafana_stack import *
from .iot_policy_stack import *
from .sidewalk_provisioning_stack import *
from .sidewalk_stack import *
from .timestream_stack import *
from .env_file_stack import *


from .workshop_studio import WorkshopStudioStack


class DemoBadgeWorkshopStack(WorkshopStudioStack):
    def __init__(self, scope: Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)

        credentials_stack = CredentialsStack(self, "Credentials")
        username = credentials_stack.credentials.get_att_string("username")
        password = credentials_stack.credentials.get_att_string("password")

        iot_policy_stack = IoTPolicyStack(self, "IoTPolicy")

        timestream_stack = TimestreamStack(self, "Timestream")

        grafana_stack = GrafanaStack(
            self, "Grafana", username=username, password=password
        )

        sidewalk_stack = SidewalkStack(self, "Sidewalk")

        sidewalk_provisioning_stack = SidewalkProvisioningStack(
            self,
            "SidewalkProvisioning",
            destination_role=sidewalk_stack.sidewalk_destination_role,
        )

        companion_web_app_stack = CompanionWebAppStack(
            self,
            "CompanionWebApp",
            sidewalk_provisioning_lambda=sidewalk_provisioning_stack.sidewalk_lambda,
            sidewalk_destination_role_arn=sidewalk_stack.sidewalk_destination_role.role_arn,
            timestream_insert_role_arn=timestream_stack.timestream_insert_role.role_arn,
            username=username,
            password=password,
        )

        env_file_stack = EnvFileStack(
            self,
            "EnvFileStack",
            sidewalk_provisioning_lambda=sidewalk_provisioning_stack.sidewalk_lambda,
            grafana_url=grafana_stack.grafana_url.value,
            identity_pool_id=companion_web_app_stack.identity_pool.identity_pool_id,
            companion_web_app_bucket=companion_web_app_stack.companion_web_app_bucket,
            companion_web_app_url=companion_web_app_stack.companion_web_app_url.value,
            ota_role_arn=companion_web_app_stack.ota_role.role_arn,
            cognito_redirect_url=companion_web_app_stack.cognito_app_redirect_url,
            cognito_policy_name=companion_web_app_stack.cognito_iot_policy.policy_name,
            user_pool_id=companion_web_app_stack.user_pool.user_pool_id,
            timestream_db_name=timestream_stack.timestream_db.database_name,
            timestream_table_name=timestream_stack.timestream_table.table_name,
            timestream_insert_role_arn=timestream_stack.timestream_insert_role.role_arn,
        )

        additional_ota_job_freertos_console_policy = aws_iam.Policy(
            self, "OTAJobFreeRTOS-Signing-WSParticipantPolicy"
        )
        p: aws_iam.CfnPolicy = (
            additional_ota_job_freertos_console_policy.node.default_child
        )
        p.cfn_options.condition = self.is_workshop_studio_condition
        additional_ota_job_freertos_console_policy.attach_to_role(
            aws_iam.Role.from_role_name(
                self, "ParticipantRole", self.participant_role_name.value_as_string
            )
        )
        additional_ota_job_freertos_console_policy.add_statements(
            aws_iam.PolicyStatement(
                effect=aws_iam.Effect.ALLOW,
                actions=[
                    "acm:ListCertificates",
                    "signer:GetSigningPlatform",
                    "signer:GetSigningProfile",
                    "signer:ListSigningPlatforms",
                    "signer:ListSigningProfiles",
                    "signer:ListSigningJobs",
                ],
                resources=["*"],
            )
        )

        cdk.CfnOutput(
            self, "Username", value=username, export_name="DemoBadgeWorkshop-Username"
        )
        cdk.CfnOutput(
            self, "Password", value=password, export_name="DemoBadgeWorkshop-Password"
        )
        cdk.CfnOutput(
            self,
            "GrafanaURL",
            value=grafana_stack.grafana_url.value,
            export_name="DemoBadgeWorkshop-GrafanaURL",
        )
        cdk.CfnOutput(
            self,
            "CompanionWebAppURL",
            value=companion_web_app_stack.companion_web_app_url.value,
            export_name="DemoBadgeWorkshop-CompanionWebAppURL",
        )
