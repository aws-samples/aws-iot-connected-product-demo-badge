# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import os
from pathlib import Path

from aws_cdk import BundlingOptions, Duration
from aws_cdk import aws_iam as _iam
from aws_cdk import aws_lambda as _lambda
from constructs import Construct

from .workshop_studio import NestedWorkshopStudioStack


class SidewalkProvisioningStack(NestedWorkshopStudioStack):
    def __init__(
        self, scope: Construct, construct_id: str, destination_role, **kwargs
    ) -> None:
        super().__init__(scope, construct_id, **kwargs)

        lambda_role = _iam.Role(
            self,
            "SidewalkProvisioningLambdaRole",
            assumed_by=_iam.ServicePrincipal("lambda.amazonaws.com"),
            inline_policies={
                "IoTWireless": _iam.PolicyDocument(
                    statements=[
                        _iam.PolicyStatement(
                            effect=_iam.Effect.ALLOW,
                            actions=[
                                "iotwireless:GetWirelessDevice",
                                "iotwireless:GetDeviceProfile",
                                "iotwireless:CreateDestination",
                                "iotwireless:CreateDeviceProfile",
                                "iotwireless:CreateWirelessDevice",
                                "iotwireless:ListDestinations",
                                "iotwireless:ListDeviceProfiles",
                                "iotwireless:ListWirelessDevices",
                            ],
                            resources=["*"],
                        ),
                        _iam.PolicyStatement(
                            effect=_iam.Effect.ALLOW,
                            actions=["iam:PassRole"],
                            resources=[destination_role.role_arn],
                        ),
                        _iam.PolicyStatement(
                            effect=_iam.Effect.ALLOW,
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

        runtime = _lambda.Runtime.PYTHON_3_11

        lambda_path = (
            Path(__file__).parents[1].joinpath("lambda", "SidewalkProvisioningLambda")
        )
        if "USE_PRE_BUNDLED_ASSETS" in os.environ:
            lambda_bundling_options = None
        else:
            lambda_bundling_options = BundlingOptions(
                image=runtime.bundling_image,
                command=[
                    "bash",
                    "-c",
                    "cd /asset-input && python3 -m pip install -r requirements.txt -t /asset-output && cp -au . /asset-output",
                ],
            )

        lambda_code = _lambda.Code.from_asset(
            path=str(lambda_path),
            bundling=lambda_bundling_options,
        )

        self.sidewalk_lambda = _lambda.Function(
            self,
            "SidewalkProvisioningLambda",
            runtime=runtime,
            code=lambda_code,
            handler="main.lambda_handler",
            role=lambda_role,
            timeout=Duration.seconds(30),
            environment={
                "DESTINATION_ROLE_ARN": destination_role.role_arn,
                "DESTINATION_MQTT_TOPIC": "sidewalk_data",
            },
        )
