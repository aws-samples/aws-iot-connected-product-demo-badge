// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sht31);

#include "badge.h"
#include "self_test.h"

K_MUTEX_DEFINE(sht3xd_mutex);
const struct device *const sht3xd_dev = DEVICE_DT_GET_ONE(sensirion_sht3xd);

int init_sht31(void) {
    if (!device_is_ready(sht3xd_dev)) {
        LOG_ERR("sensor: device not ready.");
        return -1;
    }

#ifdef CONFIG_SHT3XD_TRIGGER
    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_THRESHOLD,
        .chan = SENSOR_CHAN_HUMIDITY,
    };
    struct sensor_value lo_thr = {ALERT_HUMIDITY_LO};
    struct sensor_value hi_thr = {ALERT_HUMIDITY_HI};
    bool last_alerted = false;

    ret = sensor_attr_set(sht3xd_dev, SENSOR_CHAN_HUMIDITY, SENSOR_ATTR_LOWER_THRESH, &lo_thr);
    if (ret == 0) {
        ret = sensor_attr_set(sht3xd_dev, SENSOR_CHAN_HUMIDITY, SENSOR_ATTR_UPPER_THRESH, &hi_thr);
    }
    if (ret == 0) {
        ret = sensor_trigger_set(sht3xd_dev, &trig, trigger_handler);
    }
    if (ret != 0) {
        LOG_ERR("SHT3XD: trigger config failed: %d\n", ret);
        return -1;
    }
    LOG_ERR("Alert outside %d..%d %%RH got %d\n", lo_thr.val1, hi_thr.val1, ret);
#endif

    LOG_INF("init complete.");
    return 0;
}

#ifdef CONFIG_SHT3XD_TRIGGER
static volatile bool alerted;

static void trigger_handler(const struct device *dev, const struct sensor_trigger *trig) {
    alerted = !alerted;
}
#endif

int read_sht31_sample(sht3xd_sample *v) {
    k_mutex_lock(&sht3xd_mutex, K_FOREVER);

    struct sensor_value temp, hum;
    int ret = sensor_sample_fetch(sht3xd_dev);
    if (ret != 0) {
        return ret;
    }

    ret = sensor_channel_get(sht3xd_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (ret != 0) {
        return ret;
    }
    ret = sensor_channel_get(sht3xd_dev, SENSOR_CHAN_HUMIDITY, &hum);
    if (ret != 0) {
        return ret;
    }

    v->temperature = sensor_value_to_double(&temp) - 3.8; // with empirical offset calibration
    v->humidity = sensor_value_to_double(&hum);

    k_mutex_unlock(&sht3xd_mutex);
    return 0;
}

int run_sht31(void) {
    // https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/sensor/sht3xd

    int i = 3;
    while (i--) {
        sht3xd_sample v;
        read_sht31_sample(&v);

#ifdef CONFIG_SHT3XD_TRIGGER
        if (alerted != last_alerted) {
            if (lo_thr.val1 > hum.val1) {
                LOG_INF("ALERT: humidity %d < %d\n", hum.val1, lo_thr.val1);
            } else if (hi_thr.val1 < hum.val1) {
                LOG_INF("ALERT: humidity %d > %d\n", hum.val1, hi_thr.val1);
            } else {
                LOG_INF("ALERT: humidity %d <= %d <= %d\n", lo_thr.val1, hum.val1, hi_thr.val1);
            }
            last_alerted = alerted;
        }
#endif

        LOG_INF("SHT31: %.1f°C ; %.1f %%RH", v.temperature, v.humidity);
        k_msleep(200);
    }

    return 0;
}
