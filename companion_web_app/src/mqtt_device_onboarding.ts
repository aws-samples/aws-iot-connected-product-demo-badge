// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

import { AttachPolicyCommand, AttachPrincipalPolicyCommand, AttachThingPrincipalCommand, CreateThingCommand, RegisterCertificateWithoutCACommand, ResourceAlreadyExistsException } from "@aws-sdk/client-iot";
import { fetchIoTCoreEndpoint, getIoTClient } from "./aws_access";
import { getEnv } from "./env";
import { showToast } from './utils';

let writeToSerialPort;

export async function initMqttDeviceOnboarding(w) {
    writeToSerialPort = w

    document.getElementById('set_wifi_ssid')?.addEventListener('click', () => {
        writeToSerialPort(`\r\nexpresslink cmd AT+CONF SSID=${getEnv().WiFiSSID}\r\n`);
    });
    document.getElementById('set_wifi_passphrase')?.addEventListener('click', () => {
        writeToSerialPort(`\r\nexpresslink cmd AT+CONF Passphrase=${getEnv().WiFiPassphrase}\r\n`);
    });
    document.getElementById('set_aws_iot_core_endpoint')?.addEventListener('click', async () => {
        const endpoint = await fetchIoTCoreEndpoint(getEnv());
        showToast("Discovered AWS IoT Core endpoint!", `Your AWS IoT Core Data-ATS endpoint is: <pre>${endpoint}</pre>`);
        writeToSerialPort(`\r\nexpresslink cmd AT+CONF Endpoint=${endpoint}\r\n`);
    });
    document.getElementById('get_thing_name')?.addEventListener('click', async () => {
        writeToSerialPort(`\r\nexpresslink cmd AT+CONF? ThingName\r\n`);
    });
    document.getElementById('export_expresslink_certificate')?.addEventListener('click', async () => {
        document.getElementById('expresslink_certificate_spinner')?.classList.remove('d-none');
        writeToSerialPort(`\r\nexpresslink export_certificate\r\n`);
    });
    document.getElementById('expresslink_connect')?.addEventListener('click', async () => {
        writeToSerialPort(`\r\nexpresslink cmd AT+CONNECT\r\n`);
    });
    document.getElementById('mqtt_fast_start')?.addEventListener('click', async () => {
        const spinner = document.getElementById('mqtt_fast_start_spinner')!;
        spinner.classList.remove('d-none');
        await fastStart();
        spinner.classList.add('d-none');
    });
}

async function fastStart() {
    if (!window.confirm("Perform MQTT Device Onboarding?")) {
        return;
    }

    let d = 0;
    function delay() {
        d += 500;
        return d
    }

    setTimeout(writeToSerialPort, delay(), `\r\nresize\r\n`);

    setTimeout(writeToSerialPort, delay(), `\r\nexpresslink cmd AT+CONF SSID=${getEnv().WiFiSSID}\r\n`);

    setTimeout(writeToSerialPort, delay(), `\r\nexpresslink cmd AT+CONF Passphrase=${getEnv().WiFiPassphrase}\r\n`);

    const endpoint = await fetchIoTCoreEndpoint(getEnv());
    setTimeout(writeToSerialPort, delay(), `\r\nexpresslink cmd AT+CONF Endpoint=${endpoint}\r\n`);

    setTimeout(writeToSerialPort, delay(), `\r\nexpresslink cmd AT+CONF? ThingName\r\n`);

    setTimeout(writeToSerialPort, delay(), `\r\nexpresslink export_certificate\r\n`);

    setTimeout(onboardMQTTDevice, delay() + 2000);

    await new Promise(r => setTimeout(r, delay() + 4000));
}

async function onboardMQTTDevice() {
    const client = await getIoTClient(getEnv());

    try {
        console.log("CreateThingCommand", await client.send(new CreateThingCommand({
            thingName: getEnv().thing_name,
        })));
    } catch (error) {
        if (error instanceof ResourceAlreadyExistsException) {
            console.log("Thing already exists", error);
        } else {
            throw error;
        }
    }

    let certificateArn;
    try {
        const registerCertificateWithoutCAResponse = await client.send(new RegisterCertificateWithoutCACommand({
            certificatePem: getEnv().certificate,
            status: "ACTIVE",
        }));
        console.log("RegisterCertificateWithoutCACommand", registerCertificateWithoutCAResponse);
        certificateArn = registerCertificateWithoutCAResponse.certificateArn!;
    } catch (error) {
        if (error instanceof ResourceAlreadyExistsException) {
            console.log(error.message);
            certificateArn = error.resourceArn;
        } else {
            throw error;
        }
    }

    try {
        console.log("AttachPolicyCommand", await client.send(new AttachPolicyCommand({
            target: certificateArn,
            policyName: "DemoBadgeDevelopmentPolicy",
        })));
    } catch (error) {
        console.warn(error);
    }

    try {
        console.log("AttachThingPrincipalCommand", await client.send(new AttachThingPrincipalCommand({
            thingName: getEnv().thing_name,
            principal: certificateArn,
        })));
    } catch (error) {
        console.warn(error);
    }

    await new Promise(r => setTimeout(r, 2000));

    writeToSerialPort(`\r\nexpresslink cmd AT+CONNECT\r\n`);
}
