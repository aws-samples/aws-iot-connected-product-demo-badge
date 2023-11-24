// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

import { getEnv, env, getClientCreds } from './env'
import { fromCognitoIdentityPool } from '@aws-sdk/credential-provider-cognito-identity';
import { CognitoIdentityClient } from '@aws-sdk/client-cognito-identity';
import { CognitoIdentityCredentials } from "@aws-sdk/credential-provider-cognito-identity/dist-types/fromCognitoIdentity"
import { AttachPolicyCommand } from "@aws-sdk/client-iot"
import { auth, iot, mqtt5 } from 'aws-iot-device-sdk-v2';
import { v4 as uuid } from "uuid";
import { showToast } from './utils';
import { getIoTClient } from "./aws_access"
import { digital_twin_and_shadow_messageReceived, getInitialShadowDoc } from './digital_twin_and_shadow';

interface AWSCognitoCredentialOptions {
    IdentityPoolId: string,
    Region: string
}

class AWSCognitoCredentialsProvider extends auth.CredentialsProvider {
    private options: AWSCognitoCredentialOptions;
    private cachedCredentials?: CognitoIdentityCredentials;

    constructor(options: AWSCognitoCredentialOptions, expire_interval_in_ms?: number) {
        super();
        this.options = options;

        setInterval(async () => {
            await this.refreshCredentials();
        }, expire_interval_in_ms ?? 3600 * 1000);
    }

    getCredentials(): auth.AWSCredentials {
        //before we do anything with credentials for mqtt lets attach the policy to Identity ID ( NOT Identity Pool ID !!) first..
        this.attach_policy()
        //get credentials for mqtt client
        return {
            aws_access_id: this.cachedCredentials?.accessKeyId,
            aws_secret_key: this.cachedCredentials?.secretAccessKey,
            aws_sts_token: this.cachedCredentials?.sessionToken,
            aws_region: this.options.Region
        }
    }

async attach_policy(){
    const client = await getIoTClient(env)
    const input = {
    policyName: getEnv().CognitoIoTPolicyName,
    target: this.cachedCredentials?.identityId,
    };
    const command = new AttachPolicyCommand(input);
    await client.send(command);
}

    async refreshCredentials() {
        console.log('Fetching Cognito credentials...');
        this.cachedCredentials = await fromCognitoIdentityPool({
            // Required. The unique identifier for the identity pool from which an identity should be
            // retrieved or generated.
            identityPoolId: this.options.IdentityPoolId,
            client: new CognitoIdentityClient({ region: this.options.Region }),
            //below makes sure we are using authenticated role via user pool
            logins: getClientCreds()
        })();
    }
}

let client: mqtt5.Mqtt5Client;

export function getMqttClient() {
    return client;
}

export async function initMqtt() {
    const provider = new AWSCognitoCredentialsProvider({
        IdentityPoolId: getEnv().AWSCognitoIdentityPoolId,
        Region: getEnv().AWSRegion
    });

    await provider.refreshCredentials();

    let wsConfig: iot.WebsocketSigv4Config = {
        credentialsProvider: provider,
        region: getEnv().AWSRegion,
    }

    let builder = iot.AwsIotMqtt5ClientConfigBuilder.newWebsocketMqttBuilderWithSigv4Auth(
        getEnv().endpoint,
        wsConfig
    ).withConnectProperties({
        clientId: `companion-web-app-${uuid()}`,
        keepAliveIntervalSeconds: 30,
    })

    console.log("Connecting MQTT over WS...");
    client = new mqtt5.Mqtt5Client(builder.build());

    client.on("connectionSuccess", async (_session_present) => {
        console.log("MQTT connectionSuccess.");
        await client.subscribe({
            subscriptions: [
                { topicFilter: '$aws/events/presence/connected/+', qos: mqtt5.QoS.AtLeastOnce },
                { topicFilter: '$aws/events/presence/disconnected/+', qos: mqtt5.QoS.AtLeastOnce },
            ]
        });

        // Workshop module: Device Location
        // subscribe to all things as the actual thing name might not be known yet
        await client.subscribe({
            subscriptions: [
                { topicFilter: '$aws/device_location/+/get_position_estimate/accepted', qos: mqtt5.QoS.AtLeastOnce },
                { topicFilter: '$aws/device_location/+/get_position_estimate/rejected', qos: mqtt5.QoS.AtLeastOnce },
            ]
        });

        // Workshop module: MQTT Pub/Sub
        await client.subscribe({
            subscriptions: [
                { topicFilter: 'hello/cloud', qos: mqtt5.QoS.AtLeastOnce },
                { topicFilter: 'hello/world', qos: mqtt5.QoS.AtLeastOnce },
            ]
        });

        // Workshop module: Digital Twin and Shadow
        // subscribe to all things as the actual thing name might not be known yet
        await client.subscribe({
            subscriptions: [
                { topicFilter: `$aws/things/+/shadow/get/accepted`, qos: mqtt5.QoS.AtLeastOnce },
                { topicFilter: `$aws/things/+/shadow/get/rejected`, qos: mqtt5.QoS.AtLeastOnce },
                { topicFilter: `$aws/things/+/shadow/update/accepted`, qos: mqtt5.QoS.AtLeastOnce },
                { topicFilter: `$aws/things/+/shadow/update/rejected`, qos: mqtt5.QoS.AtLeastOnce },
                { topicFilter: 'demo_badge/sensors', qos: mqtt5.QoS.AtLeastOnce },
            ]
        });
        await getInitialShadowDoc();
    });
    client.on('attemptingConnect', (eventData: mqtt5.AttemptingConnectEvent) => {
        console.log("MQTT attemptingConnect:", eventData);
    });
    client.on('connectionFailure', (eventData: mqtt5.ConnectionFailureEvent) => {
        console.log("MQTT connection failure:", eventData.error.toString());
    });
    client.on('disconnection', (eventData: mqtt5.DisconnectionEvent) => {
        console.log("MQTT disconnection:", eventData.error.toString());
        if (eventData.disconnect !== undefined) {
            console.log('Disconnect packet:', JSON.stringify(eventData.disconnect));
        }
    });
    client.on('stopped', (eventData: mqtt5.StoppedEvent) => {
        console.log("MQTT stopped:", eventData);
    });
    client.on('error', (error) => {
        console.log("MQTT error: " + error.toString());
    });

    client.on("messageReceived", (eventData: mqtt5.MessageReceivedEvent): void => {
        const topic = eventData.message.topicName;
        if (topic.startsWith('$aws/events/presence/connected/')) {
            const r = JSON.parse(eventData.message.payload!.toString());
            if (!r.clientId.startsWith('iotconsole-')) {
                console.log("MQTT event presence connected:", eventData.message.payload?.toString());
                showToast("Thing connected to AWS IoT Core!", `Thing <strong>${r.clientId}</strong> connected from IP address <strong>${r.ipAddress}</strong> on <strong>${new Date(r.timestamp).toTimeString()}</strong>.`)
            }
        } else if (topic.startsWith('$aws/events/presence/disconnected/')) {
            const r = JSON.parse(eventData.message.payload!.toString());
            if (!r.clientId.startsWith('iotconsole-')) {
                console.log("MQTT event presence disconnected:", eventData.message.payload?.toString());
            }
        } else if (topic.match('\\$aws/device_location/.+/get_position_estimate/accepted')) {
            console.log("MQTT get_position_estimate/accepted:", eventData.message.payload?.toString());
            const r = JSON.parse(eventData.message.payload!.toString());

            document.getElementById('resolver_result')!.classList.remove('d-none');

            const mapsLink = document.getElementById('google_maps_link')! as HTMLLinkElement;
            mapsLink.href = `https://www.google.com/maps/place/${r.coordinates[1]},${r.coordinates[0]}`;
            mapsLink.textContent = `Google Maps at latitude:${r.coordinates[1]}, longitude:${r.coordinates[0]}`;

            const rawResult = document.getElementById('device_location_result') as HTMLPreElement;
            rawResult.textContent = JSON.stringify(r, null, 2);
        } else if (topic.match('\\$aws/device_location/.+/get_position_estimate/rejected')) {
            console.log("MQTT get_position_estimate/rejected:", eventData.message.payload?.toString());
        } else if (topic == 'hello/cloud') {
            console.log("MQTT hello/cloud:", eventData.message.payload?.toString());
        } else if (topic == 'hello/world') {
            console.log("MQTT hello/world:", eventData.message.payload?.toString());
        }

        digital_twin_and_shadow_messageReceived(eventData);
    });

    client.start();
}
