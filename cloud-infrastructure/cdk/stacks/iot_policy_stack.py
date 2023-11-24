# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

from aws_cdk import (
    aws_iot
)

from constructs import Construct

from .workshop_studio import NestedWorkshopStudioStack


class IoTPolicyStack(NestedWorkshopStudioStack):

    def __init__(self, scope: Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)

        # WARNING: This IoT policy has broad permissions.
        # We strongly recommend to apply 'Principle of Least Privileges' before deploying your IoT policy!
        aws_iot.CfnPolicy(self, "DemoBadgeDevelopmentPolicy",
            policy_name="DemoBadgeDevelopmentPolicy",
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
                    {
                        "Effect": "Allow",
                        "Action": [
                            "iot:Publish",
                        ],
                        "Resource": [
                            f"arn:{self.partition}:iotwireless:{self.region}:{self.account}:topic/$aws/device_location/*/get_position_estimate",
                        ],
                    },
                    {
                        "Effect": "Allow",
                        "Action": [
                            "iot:Subscribe",
                            "iot:Receive",
                        ],
                        "Resource": [
                            f"arn:{self.partition}:iotwireless:{self.region}:{self.account}:topic/$aws/device_location/*/get_position_estimate/accepted",
                            f"arn:{self.partition}:iotwireless:{self.region}:{self.account}:topic/$aws/device_location/*/get_position_estimate/rejected",
                        ],
                    },
                ]
            },
        )
