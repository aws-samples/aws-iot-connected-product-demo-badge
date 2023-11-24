// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
LOG_MODULE_REGISTER(lsm6dsl);

#include "badge.h"
#include "self_test.h"

K_MUTEX_DEFINE(lsm6dsl_mutex);
const struct device *const lsm6dsl_dev = DEVICE_DT_GET_ONE(st_lsm6dsl);

int init_lsm6dsl(void) {
    if (!device_is_ready(lsm6dsl_dev)) {
        LOG_ERR("sensor: device not ready.");
        return -1;
    }

    /* set accel/gyro sampling frequency to 104 Hz */
    struct sensor_value odr_attr;
    odr_attr.val1 = 104;
    odr_attr.val2 = 0;

    if (sensor_attr_set(lsm6dsl_dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) != 0) {
        LOG_ERR("Cannot set sampling frequency for accelerometer.");
        return -1;
    }

    if (sensor_attr_set(lsm6dsl_dev, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) != 0) {
        LOG_ERR("Cannot set sampling frequency for gyro.");
        return -1;
    }

    LOG_INF("init complete.");
    return 0;
}

int read_lsm6dsl_sample(lsm6dsl_sample *v) {
    k_mutex_lock(&lsm6dsl_mutex, K_FOREVER);

    int ret;
    struct sensor_value t;

    ret = sensor_sample_fetch_chan(lsm6dsl_dev, SENSOR_CHAN_ACCEL_XYZ);
    if (ret != 0) {
        return ret;
    }

    ret = sensor_channel_get(lsm6dsl_dev, SENSOR_CHAN_ACCEL_X, &t);
    if (ret != 0) {
        return ret;
    }
    v->accel_x = sensor_value_to_double(&t);

    ret = sensor_channel_get(lsm6dsl_dev, SENSOR_CHAN_ACCEL_Y, &t);
    if (ret != 0) {
        return ret;
    }
    v->accel_y = sensor_value_to_double(&t);

    ret = sensor_channel_get(lsm6dsl_dev, SENSOR_CHAN_ACCEL_Z, &t);
    if (ret != 0) {
        return ret;
    }
    v->accel_z = sensor_value_to_double(&t);

    ret = sensor_sample_fetch_chan(lsm6dsl_dev, SENSOR_CHAN_GYRO_XYZ);
    if (ret != 0) {
        return ret;
    }

    ret = sensor_channel_get(lsm6dsl_dev, SENSOR_CHAN_GYRO_X, &t);
    if (ret != 0) {
        return ret;
    }
    v->angular_velocity_x = sensor_value_to_double(&t);

    ret = sensor_channel_get(lsm6dsl_dev, SENSOR_CHAN_GYRO_Y, &t);
    if (ret != 0) {
        return ret;
    }
    v->angular_velocity_y = sensor_value_to_double(&t);

    ret = sensor_channel_get(lsm6dsl_dev, SENSOR_CHAN_GYRO_Z, &t);
    if (ret != 0) {
        return ret;
    }
    v->angular_velocity_z = sensor_value_to_double(&t);

    k_mutex_unlock(&lsm6dsl_mutex);
    return 0;
}

int run_lsm6dsl() {
    int i = 5;
    while (i--) {
        lsm6dsl_sample v;
        read_lsm6dsl_sample(&v);

        LOG_INF("LSM6DSL: accel x:%.1f y:%.1f z:%.1f m/s^2 | gyro x:%.3f y:%.3f z:%.3f deg/s",
                v.accel_x,
                v.accel_y,
                v.accel_z,
                v.angular_velocity_x,
                v.angular_velocity_y,
                v.angular_velocity_z);

        k_sleep(K_MSEC(200));
    }
    return 0;
}
