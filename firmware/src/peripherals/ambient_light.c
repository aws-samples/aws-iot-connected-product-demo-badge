// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ambient_light);

#include "badge.h"
#include "self_test.h"

K_MUTEX_DEFINE(ambient_light_mutex);
static const struct adc_dt_spec ambient_light_adc = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

uint16_t buf;
struct adc_sequence sequence = {
    .buffer = &buf,
    .buffer_size = sizeof(buf), // buffer size in bytes, not number of samples
};

int init_ambient_light(void) {
    if (!device_is_ready(ambient_light_adc.dev)) {
        LOG_ERR("Device %s is not ready\n", ambient_light_adc.dev->name);
        return -1;
    }

    if (adc_channel_setup_dt(&ambient_light_adc) != 0) {
        LOG_ERR("Could not setup ambient_light_adc");
        return -1;
    }

    if (adc_sequence_init_dt(&ambient_light_adc, &sequence) != 0) {
        LOG_ERR("failed to init dt");
    }

    LOG_INF("init complete.");
    return 0;
}

int16_t read_ambient_light() {
    k_mutex_lock(&ambient_light_mutex, K_FOREVER);
    int ret = adc_read(ambient_light_adc.dev, &sequence);
    if (ret != 0) {
        LOG_ERR("Could not read ambient light value: %d", ret);
        k_mutex_unlock(&ambient_light_mutex);
        return 0;
    }
    k_mutex_unlock(&ambient_light_mutex);

    int16_t result = (int16_t)buf;
    return result < 0 ? 0 : result;
}

int run_ambient_light() {
    int i = 3;
    while (i--) {
        int16_t v = read_ambient_light();
        LOG_INF("Ambient Light: %hd", v);
        k_msleep(200);
    }

    return 0;
}
