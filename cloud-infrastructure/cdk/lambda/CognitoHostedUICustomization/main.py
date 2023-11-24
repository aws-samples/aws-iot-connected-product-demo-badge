# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import json
import urllib3
import boto3

SUCCESS = "SUCCESS"
FAILED = "FAILED"

http = urllib3.PoolManager()

# function fron cfn-response module:
# https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/cfn-lambda-function-code-cfnresponsemodule.html#cfn-lambda-function-code-cfnresponsemodule-source
def send(event, context, responseStatus, responseData, physicalResourceId=None, noEcho=False, reason=None):
    responseUrl = event['ResponseURL']
    print(responseUrl)

    responseBody = {
        'Status' : responseStatus,
        'Reason' : reason or "See the details in CloudWatch Log Stream: {}".format(context.log_stream_name),
        'PhysicalResourceId' : physicalResourceId or context.log_stream_name,
        'StackId' : event['StackId'],
        'RequestId' : event['RequestId'],
        'LogicalResourceId' : event['LogicalResourceId'],
        'NoEcho' : noEcho,
        'Data' : responseData
    }

    json_responseBody = json.dumps(responseBody)

    print("Response body:")
    print(json_responseBody)

    headers = {
        'content-type': '',
        'content-length': str(len(json_responseBody))
    }

    try:
        response = http.request('PUT', responseUrl, headers=headers, body=json_responseBody)
        print("Status code:", response.status)
    except Exception as e:
        print("send(..) failed executing http.request(..):", e)

def lambda_handler(event, context):
    print('REQUEST RECEIVED:\\n' + json.dumps(event))
    responseStatus = FAILED
    responseData = {}
    if 'RequestType' not in event:
        responseData = {'error': 'RequestType not in event'}
    elif event['RequestType'] == 'Delete':
        responseStatus = SUCCESS
    elif event['RequestType'] in ['Create', 'Update']:
        try:
            with open('cognito-logo.png', 'rb') as f:
                image = f.read()
            print(f"Image has {len(image)} bytes.")

            client = boto3.client('cognito-idp')
            response = client.set_ui_customization(
                UserPoolId=event['ResourceProperties']['UserPoolId'],
                ClientId=event['ResourceProperties']['ClientId'],
                ImageFile=image,
                # CSS='string',
            )
            print('done')
            print(response)

            responseStatus = SUCCESS
        except Exception as e:
            responseData = {'error': str(e)}
    send(event, context, responseStatus, responseData)
