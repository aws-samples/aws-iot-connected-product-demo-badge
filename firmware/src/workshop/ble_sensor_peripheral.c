// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ble_sensor_peripheral);

#include "badge.h"

void ble_sensor_peripheral(void *context, void *dummy1, void *dummy2) {
    char cmd[128];
    char expresslink_response[128];

    expresslink_reset();

    snprintf(cmd, sizeof(cmd), "AT+CONF BLEPeripheral={\"appearance\": \"4142\"}\n");
    expresslink_send_command(cmd, NULL, 0);

    snprintf(cmd, sizeof(cmd), "AT+CONF BLEGATT1={\"service\": \"181A\", \"chr\": \"2A6E\", \"read\":1, \"notify\":1 }\n");
    expresslink_send_command(cmd, NULL, 0);

    snprintf(cmd, sizeof(cmd), "AT+CONF BLEGATT2={\"service\": \"181A\", \"chr\": \"2A6F\", \"read\":1, \"notify\":1 }\n");
    expresslink_send_command(cmd, NULL, 0);

    snprintf(cmd, sizeof(cmd), "AT+BLE INIT PERIPHERAL\n");
    expresslink_send_command(cmd, NULL, 0);

    expresslink_send_command("AT+BLE ADVERTISE\n", NULL, 0);

    int64_t last_update_time = k_uptime_get();

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'BLE Sensor Peripheral' module.");
            expresslink_reset();
            display_handler();
            return;
        }

        display_handler();

        if (expresslink_check_event_pending()) {
            expresslink_send_command("AT+EVENT?\n", expresslink_response, sizeof(expresslink_response));
            LOG_INF("%s", expresslink_response);
            if (expresslink_is_event(expresslink_response, EL_EVENT_BLE_CONNECTION_LOST)) {
                expresslink_send_command("AT+BLE ADVERTISE\n", NULL, 0);
            }
        }

        if (k_uptime_get() > last_update_time + 5000) {
            sht3xd_sample v;
            read_sht31_sample(&v);

            snprintf(cmd, sizeof(cmd), "AT+BLE SET1 %04x\n", (uint16_t)v.temperature);
            expresslink_send_command(cmd, NULL, 0);

            snprintf(cmd, sizeof(cmd), "AT+BLE SET2 %04x\n", (uint16_t)v.humidity);
            expresslink_send_command(cmd, NULL, 0);

            last_update_time = k_uptime_get();
        }
    }
}
