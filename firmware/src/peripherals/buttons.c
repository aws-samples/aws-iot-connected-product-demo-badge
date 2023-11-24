// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(buttons);

#include "badge.h"
#include "self_test.h"

static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(DT_ALIAS(button1), gpios);
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(DT_ALIAS(button2), gpios);
static const struct gpio_dt_spec button3 = GPIO_DT_SPEC_GET(DT_ALIAS(button3), gpios);
static const struct gpio_dt_spec button4 = GPIO_DT_SPEC_GET(DT_ALIAS(button4), gpios);

static struct gpio_callback button_cb_data1;
static struct gpio_callback button_cb_data2;
static struct gpio_callback button_cb_data3;
static struct gpio_callback button_cb_data4;

volatile bool button1_pressed = false;
volatile bool button2_pressed = false;
volatile bool button3_pressed = false;
volatile bool button4_pressed = false;

void button_pressed_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    int button = 0;
    if (pins == BIT(button1.pin)) {
        button1_pressed = true;
        button = 1;
    } else if (pins == BIT(button2.pin)) {
        button2_pressed = true;
        button = 2;
    } else if (pins == BIT(button3.pin)) {
        button3_pressed = true;
        button = 3;
    } else if (pins == BIT(button4.pin)) {
        button4_pressed = true;
        button = 4;
    } else {
        LOG_WRN("Unknown button pressed!");
    }

    LOG_INF("Button %d pressed!", button);
}

int init_buttons(void) {
    if (!gpio_is_ready_dt(&button1)) {
        LOG_ERR("button1 device not ready");
        return -1;
    }
    if (!gpio_is_ready_dt(&button2)) {
        LOG_ERR("button2 device not ready");
        return -1;
    }
    if (!gpio_is_ready_dt(&button3)) {
        LOG_ERR("button3 device not ready");
        return -1;
    }
    if (!gpio_is_ready_dt(&button4)) {
        LOG_ERR("button4 device not ready");
        return -1;
    }

    if (gpio_pin_configure_dt(&button1, GPIO_INPUT) != 0) {
        LOG_ERR("button1 gpio pin configure failed");
        return -1;
    }
    if (gpio_pin_configure_dt(&button2, GPIO_INPUT) != 0) {
        LOG_ERR("button2 gpio pin configure failed");
        return -1;
    }
    if (gpio_pin_configure_dt(&button3, GPIO_INPUT) != 0) {
        LOG_ERR("button3 gpio pin configure failed");
        return -1;
    }
    if (gpio_pin_configure_dt(&button4, GPIO_INPUT) != 0) {
        LOG_ERR("button4 gpio pin configure failed");
        return -1;
    }

    if (gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_TO_ACTIVE) != 0) {
        LOG_ERR("button1 interrupt configure failed");
        return -1;
    }
    if (gpio_pin_interrupt_configure_dt(&button2, GPIO_INT_EDGE_TO_ACTIVE) != 0) {
        LOG_ERR("button2 interrupt configure failed");
        return -1;
    }
    if (gpio_pin_interrupt_configure_dt(&button3, GPIO_INT_EDGE_TO_ACTIVE) != 0) {
        LOG_ERR("button3 interrupt configure failed");
        return -1;
    }
    if (gpio_pin_interrupt_configure_dt(&button4, GPIO_INT_EDGE_TO_ACTIVE) != 0) {
        LOG_ERR("button4 interrupt configure failed");
        return -1;
    }

    gpio_init_callback(&button_cb_data1, button_pressed_cb, BIT(button1.pin));
    gpio_init_callback(&button_cb_data2, button_pressed_cb, BIT(button2.pin));
    gpio_init_callback(&button_cb_data3, button_pressed_cb, BIT(button3.pin));
    gpio_init_callback(&button_cb_data4, button_pressed_cb, BIT(button4.pin));

    if (gpio_add_callback(button1.port, &button_cb_data1) != 0) {
        LOG_ERR("button1 gpio add callback failed");
        return -1;
    }
    if (gpio_add_callback(button2.port, &button_cb_data2) != 0) {
        LOG_ERR("button2 gpio add callback failed");
        return -1;
    }
    if (gpio_add_callback(button3.port, &button_cb_data3) != 0) {
        LOG_ERR("button3 gpio add callback failed");
        return -1;
    }
    if (gpio_add_callback(button4.port, &button_cb_data4) != 0) {
        LOG_ERR("button4 gpio add callback failed");
        return -1;
    }

    LOG_INF("init complete.");
    return 0;
}
