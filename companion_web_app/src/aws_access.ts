// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

import { CognitoIdentityClient } from "@aws-sdk/client-cognito-identity";
import { fromCognitoIdentityPool } from "@aws-sdk/credential-provider-cognito-identity";
import { IoTClient, DescribeEndpointCommand } from "@aws-sdk/client-iot";
import { IoTDataPlaneClient } from '@aws-sdk/client-iot-data-plane';
import { getClientCreds } from './env';

let iotDataEndpoint: string | null = null;
let iotClient: IoTClient | null = null;
let iotDataClient: IoTDataPlaneClient | null = null;

export async function getIoTClient(env) {
    if (iotClient !== null) {
        return iotClient;
    }

    iotClient = new IoTClient({
        region: env.AWSRegion,
        credentials: fromCognitoIdentityPool({
            client: new CognitoIdentityClient({ region: env.AWSRegion }),
            identityPoolId: env.AWSCognitoIdentityPoolId,
            logins: getClientCreds(),
        }),
    });
    return iotClient;
}

export async function getIoTDataClient(env) {
    if (iotDataClient !== null) {
        return iotDataClient;
    }

    iotDataClient = new IoTDataPlaneClient({
        endpoint: env.endpoint,
        region: env.AWSRegion,
    });
    return iotDataClient;
}

export async function fetchIoTCoreEndpoint(env) {
    if (iotDataEndpoint !== null) {
        return iotDataEndpoint;
    }

    const iotClient = await getIoTClient(env);
    const command = new DescribeEndpointCommand({
        endpointType: 'iot:Data-ATS',
    });
    const response = await iotClient.send(command);
    iotDataEndpoint = response['endpointAddress']!;
    return iotDataEndpoint;
}
