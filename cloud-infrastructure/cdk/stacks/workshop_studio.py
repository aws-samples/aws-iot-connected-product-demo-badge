# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import aws_cdk as cdk
from constructs import Construct

def add_workshop_studio_parameters(self):
    self.participant_role_name = cdk.CfnParameter(self, "ParticipantRoleName", default="")
    self.participant_role_arn = cdk.CfnParameter(self, "ParticipantRoleArn", default="")
    self.participant_assumed_role_session_name = cdk.CfnParameter(self, "ParticipantAssumedRoleSessionName", default="")
    self.participant_assumed_role_arn = cdk.CfnParameter(self, "ParticipantAssumedRoleArn", default="")
    self.assets_bucket_name = cdk.CfnParameter(self, "AssetsBucketName", default="")
    self.assets_bucket_prefix = cdk.CfnParameter(self, "AssetsBucketPrefix", default="")
    self.ec2_key_pair_name = cdk.CfnParameter(self, "EC2KeyPairName", default="")

    self.parameters = {
        "ParticipantRoleName": self.participant_role_name.value_as_string,
        "ParticipantRoleArn": self.participant_role_arn.value_as_string,
        "ParticipantAssumedRoleSessionName": self.participant_assumed_role_session_name.value_as_string,
        "ParticipantAssumedRoleArn": self.participant_assumed_role_arn.value_as_string,
        "AssetsBucketName": self.assets_bucket_name.value_as_string,
        "AssetsBucketPrefix": self.assets_bucket_prefix.value_as_string,
        "EC2KeyPairName": self.ec2_key_pair_name.value_as_string,
    }

    self.is_workshop_studio_condition = cdk.CfnCondition(self, "IsWorkshopStudio",
        expression=cdk.Fn.condition_not(cdk.Fn.condition_equals(self.participant_role_name, ""))
    )


class WorkshopStudioStack(cdk.Stack):
    def __init__(self, scope: Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)
        add_workshop_studio_parameters(self)


class NestedWorkshopStudioStack(cdk.NestedStack):
    def __init__(self, scope: Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, parameters=scope.parameters, **kwargs)
        add_workshop_studio_parameters(self)
