// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

export async function initDeviceLocation(writeToSerialPort) {
    document.getElementById('run_device_location')?.addEventListener('click', async () => {
        writeToSerialPort(`\r\nworkshop run device_location\r\n`);
    });

    document.getElementById('wifi_scan')?.addEventListener('click', () => {
        writeToSerialPort(`\r\nexpresslink cmd AT+DIAG WIFI SCAN workshop MacAddress Rss\r\n`);
    })
}
