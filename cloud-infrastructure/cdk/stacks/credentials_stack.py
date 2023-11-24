# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import textwrap

import aws_cdk as cdk
from aws_cdk import (
    aws_lambda,
)
from constructs import Construct

from .workshop_studio import NestedWorkshopStudioStack


class CredentialsStack(NestedWorkshopStudioStack):

    def __init__(self, scope: Construct, construct_id: str, **kwargs) -> None:
        super().__init__(scope, construct_id, **kwargs)

        credentials_generator_lambda = aws_lambda.Function(self, "CredentialsGeneratorLambda",
            runtime=aws_lambda.Runtime.PYTHON_3_11,
            handler="index.lambda_handler",
            timeout=cdk.Duration.seconds(15),
            code=aws_lambda.Code.from_inline(textwrap.dedent("""
                import cfnresponse
                import secrets
                import string
                import json
                def lambda_handler(event, context):
                    print('REQUEST RECEIVED: ' + json.dumps(event))
                    responseStatus = cfnresponse.FAILED
                    responseData = {}
                    if 'RequestType' not in event:
                        responseData = {'error': 'RequestType not in event'}
                    elif event['RequestType'] == 'Delete':
                        responseStatus = cfnresponse.SUCCESS
                    elif event['RequestType'] in ['Create', 'Update']:
                        alphabet = string.ascii_letters + string.digits
                        password = ''.join(secrets.choice(alphabet) for i in range(8))
                        responseData['username'] = 'workshop'
                        responseData['password'] = password
                        responseStatus = cfnresponse.SUCCESS
                    cfnresponse.send(event, context, responseStatus, responseData)
            """)),
        )

        self.credentials = credentials_generator_resource = cdk.CustomResource(
            self,
            "CredentialsGeneratorResource",
            service_token=credentials_generator_lambda.function_arn
        )

        cdk.CfnOutput(self, "Username", value=credentials_generator_resource.get_att_string("username"))
        cdk.CfnOutput(self, "Password", value=credentials_generator_resource.get_att_string("password"))
