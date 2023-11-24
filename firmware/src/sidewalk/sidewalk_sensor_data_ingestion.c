// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#if CONFIG_SIDEWALK

#include <assert.h>

#include <sid_api.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sidewalk_sensor_data);

#include "badge.h"
#include "sidewalk/sidewalk.h"
#include "sidewalk/sidewalk_ui_display.h"

#define MAX_MSG_PAYLOAD_SIZE 3

typedef struct sidewalk_sensor_data_payload {
    uint8_t message_type;
    float32_t temperature;
    float32_t humidity;
    int16_t light;
} __attribute__((packed)) sidewalk_sensor_data_payload;

int64_t sidewalk_last_updated_age = 0;

void sm_notify_sensor_data(app_context_t *app_context, bool button_pressed) {
    assert(app_context);

	sht3xd_sample v;
    read_sht31_sample(&v);

	int16_t light = read_ambient_light();

    sidewalk_sensor_data_payload payload;
    payload.message_type = 0x42;
    payload.temperature = v.temperature;
    payload.humidity = v.humidity;
    payload.light = light;

    struct sid_msg msg = {
        .data = &payload,
        .size = sizeof(payload),
    };

    struct sid_msg_desc msg_desc = {
        .link_type = SID_LINK_TYPE_1,
        .type = SID_MSG_TYPE_NOTIFY,
        .link_mode = SID_LINK_MODE_CLOUD,
        .msg_desc_attr = {
            .tx_attr = {
                .request_ack = false,
                .num_retries = 0,
            },
        },
    };

    LOG_INF("Sending sensor data message...");

    sm_send_msg(app_context, &msg_desc, &msg);

	sidewalk_update_ui_display(v.temperature, v.humidity, light);
}
#endif
