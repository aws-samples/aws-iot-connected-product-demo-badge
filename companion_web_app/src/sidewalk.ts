// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

import { CreateDestinationCommand, CreateDeviceProfileCommand, CreateWirelessDeviceCommand, DeleteDestinationCommand, DeleteDeviceProfileCommand, DeleteWirelessDeviceCommand, DeregisterWirelessDeviceCommand, GetDeviceProfileCommand, IoTWirelessClient, ListDestinationsCommand, ListDeviceProfilesCommand, ListWirelessDevicesCommand, SendDataToWirelessDeviceCommand } from "@aws-sdk/client-iot-wireless";
import { getEnv, getClientCreds } from "./env";
import { CognitoIdentityClient } from '@aws-sdk/client-cognito-identity';
import { fromCognitoIdentityPool } from "@aws-sdk/credential-provider-cognito-identity";
import { LambdaClient, InvokeCommand } from "@aws-sdk/client-lambda";
import { Destination } from "@aws-sdk/client-iot";

let wireless_device_id: string;
let wireless_device_name: string;
let wireless_device_profile_id: string;
let wireless_device_destination_name: string;

let sidewalk_device_spinner: HTMLSpanElement;
let sidewalk_device_profile_spinner: HTMLSpanElement;
let sidewalk_destination_spinner: HTMLSpanElement;
let mfgspinner: HTMLSpanElement;
let send_message_to_sidewalk_device_spinner: HTMLSpanElement;

let run_sidewalk_button: HTMLButtonElement;
let generate_sidewalk_mfg_button: HTMLButtonElement;
let send_message_to_sidewalk_device_button: HTMLButtonElement;

let client: IoTWirelessClient;

let writeToSerialPort;

export async function initSidewalk(w) {
    writeToSerialPort = w;

    (document.getElementById('open_grafana_sidewalk') as HTMLAnchorElement).href = getEnv().GrafanaURL;

    sidewalk_device_spinner = document.getElementById('sidewalk_device_spinner')! as HTMLSpanElement;
    sidewalk_device_profile_spinner = document.getElementById('sidewalk_device_profile_spinner')! as HTMLSpanElement;
    sidewalk_destination_spinner = document.getElementById('sidewalk_destination_spinner')! as HTMLSpanElement;
    mfgspinner = document.getElementById('mfg_spinner')! as HTMLSpanElement;
    send_message_to_sidewalk_device_spinner = document.getElementById('send_message_to_sidewalk_device_spinner')! as HTMLSpanElement;

    run_sidewalk_button = document.getElementById('run_sidewalk')! as HTMLButtonElement;
    generate_sidewalk_mfg_button = document.getElementById('generate_sidewalk_mfg')! as HTMLButtonElement;
    send_message_to_sidewalk_device_button = document.getElementById('send_message_to_sidewalk_device')! as HTMLButtonElement;

    document.getElementById('low_power_devices_with_amazon_sidewalk_button')?.addEventListener('click', async (event) => {
        getDeviceMetadata();
    });
    document.getElementById('sidewalk_id_button')?.addEventListener('click', async () => {
        getDeviceMetadata();
    });
    generate_sidewalk_mfg_button?.addEventListener('click', async () => {
        generateSidewalkMFG();
    });
    run_sidewalk_button?.addEventListener('click', async () => {
        writeToSerialPort(`\r\nworkshop run sidewalk\r\n`);
    });
    send_message_to_sidewalk_device_button?.addEventListener('click', async () => {
        sendMessageToDevice();
    });
    document.getElementById('sidewalk_fast_start')?.addEventListener('click', async () => {
        const spinner = document.getElementById('sidewalk_fast_start_spinner')!;
        spinner.classList.remove('d-none');
        await fastStart();
        spinner.classList.add('d-none');
    })
}

// get the device id, profile id and associated destination name from iot:wireless
async function getDeviceMetadata() {
    sidewalk_device_spinner.classList.remove('d-none');
    sidewalk_device_profile_spinner.classList.remove('d-none');
    sidewalk_destination_spinner.classList.remove('d-none');

    client = new IoTWirelessClient({
        region: getEnv().AWSRegion,
        credentials: fromCognitoIdentityPool({
            client: new CognitoIdentityClient({ region: getEnv().AWSRegion }),
            identityPoolId: getEnv().AWSCognitoIdentityPoolId,
            logins: getClientCreds(),
        }),
    });

    const response = await client.send(new ListWirelessDevicesCommand({
        MaxResults: 5,
    }));
    if (response.$metadata.httpStatusCode != 200 || response.WirelessDeviceList?.length != 1) {
        sidewalk_device_spinner.classList.add('d-none');
        sidewalk_device_profile_spinner.classList.add('d-none');
        sidewalk_destination_spinner.classList.add('d-none');
        window.alert(`You need EXACTLY one Amazon Sidewalk device provisioned in your AWS account, but found ${response.WirelessDeviceList?.length}!\n\nCheck AWS IoT Core console!`)
        return;
    }

    const device = response.WirelessDeviceList.at(0)!;
    wireless_device_id = device.Id!;
    wireless_device_name = device.Name!;
    wireless_device_profile_id = device.Sidewalk?.DeviceProfileId!;
    wireless_device_destination_name = device.DestinationName!;

    const profileResponse = await client.send(new GetDeviceProfileCommand({
        Id: wireless_device_profile_id,
    }));

    const sidewalk_device_element = document.getElementById('sidewalk_device')!
    sidewalk_device_element.replaceChildren(...[
        document.createTextNode(wireless_device_name),
        document.createElement('br'),
        document.createTextNode(`(ID: ${wireless_device_id})`),
    ]);
    sidewalk_device_element.classList.remove('d-none');

    const sidewalk_device_profile_element = document.getElementById('sidewalk_device_profile')!
    sidewalk_device_profile_element.replaceChildren(...[
        document.createTextNode(profileResponse.Name!),
        document.createElement('br'),
        document.createTextNode(`(ID: ${wireless_device_profile_id})`),
    ]);
    sidewalk_device_profile_element.classList.remove('d-none');

    const sidewalk_destination_element = document.getElementById('sidewalk_destination')!
    sidewalk_destination_element.textContent = wireless_device_destination_name;
    sidewalk_destination_element.classList.remove('d-none');

    generate_sidewalk_mfg_button.disabled = false;
    send_message_to_sidewalk_device_button.disabled = false;

    console.log("iotwireless: Sidewalk Device:", wireless_device_name, wireless_device_id);
    console.log("iotwireless: Sidewalk Device Profile:", profileResponse.Name!, wireless_device_profile_id);
    console.log("iotwireless: Destination Name:", wireless_device_destination_name);

    sidewalk_device_spinner.classList.add('d-none');
    sidewalk_device_profile_spinner.classList.add('d-none');
    sidewalk_destination_spinner.classList.add('d-none');
}

// call lambda via REST API with pulled details in order to get the bin file
async function generateSidewalkMFG() {
    if (!wireless_device_destination_name || !wireless_device_id || !wireless_device_profile_id) {
        window.alert("Get the Sidewalk device ID's first!")
        return
    }
    mfgspinner.classList.remove('d-none');

    const payload = {
        path: "/sidewalk_provision",
        httpMethod: "POST",
        body: JSON.stringify({
            "destination_name": wireless_device_destination_name,
            "device_profile_id": wireless_device_profile_id,
            "wireless_device_id": wireless_device_id
        }),
    };

    console.log(JSON.stringify({
        "destination_name": wireless_device_destination_name,
        "device_profile_id": wireless_device_profile_id,
        "wireless_device_id": wireless_device_id,
    }))

    const client = new LambdaClient({
        region: getEnv().AWSRegion,
        credentials: fromCognitoIdentityPool({
            client: new CognitoIdentityClient({ region: getEnv().AWSRegion }),
            identityPoolId: getEnv().AWSCognitoIdentityPoolId,
            logins: getClientCreds(),
        }),
    });
    const command = new InvokeCommand({
        FunctionName: getEnv().SidewalkProvisioningLambdaFunctionName,
        Payload: JSON.stringify(payload),
    });

    const { Payload, StatusCode } = await client.send(command);
    if (StatusCode != 200) {
        console.error("failed to call lambda!", StatusCode, Payload);
    }
    const result = JSON.parse(new TextDecoder().decode(Payload))
    let mfgPayload = result['body'];
    if (result['isBase64Encoded'] === false) {
        mfgPayload = mfgPayload.toString('base64');
    }

    const a = document.createElement("a");
    a.href = "data:application/octet-stream;base64," + mfgPayload;
    a.download = "amazon_sidewalk_mfg.bin";
    a.click();

    run_sidewalk_button.disabled = false;

    const res = await fetch(a.href);
    const binaryPayload = new Uint8Array(await res.arrayBuffer());
    await injectMFGPayloadViaSerial(binaryPayload);

    mfgspinner.classList.add('d-none');
}

async function injectMFGPayloadViaSerial(binaryPayload: Uint8Array) {
    writeToSerialPort("\r\nresize\r\n");
    await new Promise(r => setTimeout(r, 250));

    const cmd_start = `\r\sidewalk_provisioning `;

    const chunkSize = 100; // to match the maximum Zephyr shell argument length that works
    for (let i = 0; i < binaryPayload.length; i += chunkSize) {
        let cmd = cmd_start;
        if (i == 0) {
            // start a new file
            cmd = cmd.concat("-s ");
        } else {
            // append to existing file
            cmd = cmd.concat("-a ");
        }
        const chunk = binaryPayload.slice(i, i + chunkSize);
        for (let j = 0; j < chunk.length; j++) {
            cmd = cmd.concat(chunk[j].toString(16).padStart(2, '0'));
        }

        cmd = cmd.concat("\r\n");
        writeToSerialPort(cmd);
        await new Promise(r => setTimeout(r, 250));
    }

    // end the file
    writeToSerialPort(cmd_start + '-e\r\n');
}

async function sendMessageToDevice() {
    send_message_to_sidewalk_device_spinner.classList.remove('d-none');

    client = new IoTWirelessClient({
        region: getEnv().AWSRegion,
        credentials: fromCognitoIdentityPool({
            client: new CognitoIdentityClient({ region: getEnv().AWSRegion }),
            identityPoolId: getEnv().AWSCognitoIdentityPoolId,
            logins: getClientCreds(),
        }),
    });

    const message = `hello from the cloud: ${Date.now()}`;
    const payload = btoa(String.fromCodePoint(...new TextEncoder().encode(message)));

    const response = await client.send(new SendDataToWirelessDeviceCommand({
        Id: wireless_device_id,
        TransmitMode: 1,
        PayloadData: payload,
        WirelessMetadata: {
            Sidewalk: {
                MessageType: "CUSTOM_COMMAND_ID_NOTIFY",
                AckModeRetryDurationSecs: 180,
            }
        }
    }));
    console.log("SendDataToWirelessDeviceCommand", response);

    send_message_to_sidewalk_device_spinner.classList.add('d-none');
}

async function fastStart() {
    if (!window.confirm("Perform Sidewalk Device Onboarding? All existing resources will be deleted!")) {
        return;
    }

    client = new IoTWirelessClient({
        region: getEnv().AWSRegion,
        credentials: fromCognitoIdentityPool({
            client: new CognitoIdentityClient({ region: getEnv().AWSRegion }),
            identityPoolId: getEnv().AWSCognitoIdentityPoolId,
            logins: getClientCreds(),
        }),
    });

    const listWirelessDevicesResponse = await client.send(new ListWirelessDevicesCommand({
        WirelessDeviceType: "Sidewalk",
        MaxResults: 100,
    }));
    console.log("ListWirelessDevicesCommand", listWirelessDevicesResponse);
    for await (const element of listWirelessDevicesResponse.WirelessDeviceList!) {
        try {
            console.log("DeregisterWirelessDeviceCommand", await client.send(new DeregisterWirelessDeviceCommand({
                Identifier: element.Id,
                WirelessDeviceType: "Sidewalk",
            })));
            await new Promise(r => setTimeout(r, 1000));
        } catch {
            console.log("Skipping deregistration of device:", element.Id);
        }
        console.log("DeleteWirelessDeviceCommand", await client.send(new DeleteWirelessDeviceCommand({
            Id: element.Id,
        })));
        await new Promise(r => setTimeout(r, 1000));
    }

    const listDeviceProfilesResponse = await client.send(new ListDeviceProfilesCommand({
        DeviceProfileType: "Sidewalk",
        MaxResults: 100,
    }));
    console.log("ListDeviceProfilesCommand", listDeviceProfilesResponse);
    for await (const element of listDeviceProfilesResponse.DeviceProfileList!) {
        console.log("DeleteDeviceProfileCommand", await client.send(new DeleteDeviceProfileCommand({
            Id: element.Id,
        })));
        await new Promise(r => setTimeout(r, 1000));
    }

    const listDestinationsResponse = await client.send(new ListDestinationsCommand({
        MaxResults: 100,
    }));
    console.log("ListDestinationsCommand", listDestinationsResponse);
    for await (const element of listDestinationsResponse.DestinationList!) {
        console.log("DeleteDestinationCommand", await client.send(new DeleteDestinationCommand({
            Name: element.Name,
        })));
        await new Promise(r => setTimeout(r, 1000));
    }

    const createDestinationResponse = await client.send(new CreateDestinationCommand({
        Name: "demo_badge_sidewalk_destination",
        ExpressionType: "RuleName",
        Expression: "sidewalk_uplink_rule",
        RoleArn: `arn:aws:iam::${getEnv().AWSAccountId}:role/DemoBadgeSidewalkDestinationRole`,
    }));
    console.log("CreateDestinationCommand", createDestinationResponse);

    const createDeviceProfileResponse = await client.send(new CreateDeviceProfileCommand({
        Name: "demo_badge_device_profile",
        Sidewalk: {},
    }));
    console.log("CreateDeviceProfileCommand", createDeviceProfileResponse);

    await new Promise(r => setTimeout(r, 3000));

    const createWirelessDeviceResponse = await client.send(new CreateWirelessDeviceCommand({
        Type: "Sidewalk",
        Name: getEnv().thing_name || 'demo_badge',
        DestinationName: createDestinationResponse.Name,
        Sidewalk: {
            DeviceProfileId: createDeviceProfileResponse.Id,
        }
    }));
    console.log("CreateWirelessDeviceCommand", createWirelessDeviceResponse);

    await new Promise(r => setTimeout(r, 1000));

    await getDeviceMetadata();

    await new Promise(r => setTimeout(r, 1000));

    await generateSidewalkMFG();

    await new Promise(r => setTimeout(r, 1000));

    writeToSerialPort(`\r\nkernel reboot warm\r\n`);

    await new Promise(r => setTimeout(r, 8000));

    writeToSerialPort(`\r\nworkshop run sidewalk\r\n`);
}
