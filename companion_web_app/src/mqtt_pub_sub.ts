// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

export async function initMqttPubSub(writeToSerialPort) {
    document.getElementById('run_mqtt_pub_sub')?.addEventListener('click', async () => {
        writeToSerialPort(`\r\nworkshop run mqtt_pub_sub\r\n`);
    });
}
