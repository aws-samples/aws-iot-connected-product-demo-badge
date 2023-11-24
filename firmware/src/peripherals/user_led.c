// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(user_led);

#include "badge.h"

static const struct gpio_dt_spec user_led = GPIO_DT_SPEC_GET(DT_ALIAS(user_led), gpios);

int init_user_led(void) {
    if (!gpio_is_ready_dt(&user_led)) {
        LOG_ERR("device not ready, aborting test");
        return -1;
    }

    if (gpio_pin_configure_dt(&user_led, GPIO_OUTPUT_ACTIVE) != 0) {
        LOG_ERR("gpio pin configure failed, aborting test");
        return -1;
    }

    for (size_t i = 0; i < 4; i++) {
        gpio_pin_toggle_dt(&user_led);
        k_msleep(150);
    }
    gpio_pin_set_dt(&user_led, 1); // 1 is off, 0 is on

    LOG_INF("init complete.");
    return 0;
}

void turn_user_led_on() {
    gpio_pin_set_dt(&user_led, 0); // 0 is on
}

void turn_user_led_off() {
    gpio_pin_set_dt(&user_led, 1); // 1 is off
}

void toggle_user_led() {
    gpio_pin_toggle_dt(&user_led);
}

int run_user_led(void) {
    while (true) {
        turn_user_led_off();
        k_msleep(250);
        turn_user_led_on();
        k_msleep(250);
        toggle_user_led();
        k_msleep(250);
        toggle_user_led();
        k_msleep(250);
        toggle_user_led();
        k_msleep(2000);
    }
}
