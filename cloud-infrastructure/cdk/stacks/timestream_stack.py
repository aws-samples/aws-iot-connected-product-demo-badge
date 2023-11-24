# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import aws_cdk as cdk
from aws_cdk import (
    aws_iam
)

from constructs import Construct

from .workshop_studio import NestedWorkshopStudioStack


class TimestreamStack(NestedWorkshopStudioStack):

    def __init__(self, scope: Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)

        self.timestream_db = cdk.aws_timestream.CfnDatabase(self, "SensorData", database_name="SensorData")
        self.timestream_db.apply_removal_policy(cdk.RemovalPolicy.DESTROY, apply_to_update_replace_policy=True)

        self.timestream_table = cdk.aws_timestream.CfnTable(
            self,
            "Badge",
            database_name=self.timestream_db.database_name,
            table_name="Badge",
            retention_properties={
                "MemoryStoreRetentionPeriodInHours": "24",
                "MagneticStoreRetentionPeriodInDays": "7",
            }
        )
        self.timestream_table.apply_removal_policy(cdk.RemovalPolicy.DESTROY, apply_to_update_replace_policy=True)
        self.timestream_table.add_dependency(self.timestream_db)

        self.timestream_insert_role = aws_iam.Role(self, "TimestreamInsertRole", assumed_by=aws_iam.ServicePrincipal("iot.amazonaws.com"))
        self.timestream_insert_role.add_to_policy(aws_iam.PolicyStatement(
            effect=aws_iam.Effect.ALLOW,
            actions=[
                "timestream:WriteRecords",
            ],
            resources=[f"arn:{self.partition}:timestream:{self.region}:{self.account}:database/{self.timestream_db.database_name}/table/{self.timestream_table.table_name}"],
        ))
        self.timestream_insert_role.add_to_policy(aws_iam.PolicyStatement(
            effect=aws_iam.Effect.ALLOW,
            actions=[
                "timestream:DescribeEndpoints",
            ],
            resources=["*"],
        ))

        additional_timestream_policy = aws_iam.Policy(self, "TimestreamAdditionalPassRoleWSParticipantPolicy")
        p: aws_iam.CfnPolicy = additional_timestream_policy.node.default_child
        p.cfn_options.condition = self.is_workshop_studio_condition
        additional_timestream_policy.attach_to_role(aws_iam.Role.from_role_name(self, "ParticipantRole", self.participant_role_name.value_as_string))
        additional_timestream_policy.add_statements(aws_iam.PolicyStatement(
            effect=aws_iam.Effect.ALLOW,
            actions=[
                "iam:PassRole",
            ],
            resources=[self.timestream_insert_role.role_arn],
        ))
