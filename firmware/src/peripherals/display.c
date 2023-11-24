// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
LOG_MODULE_REGISTER(display);

#include "app_version.h"
#include "badge.h"
#include "self_test.h"

const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
const struct device *led_pwm = DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(pwm_leds));

static lv_obj_t *picture;
static lv_obj_t *qr;

K_MUTEX_DEFINE(display_handler_mutex);

int init_display(void) {
    if (!device_is_ready(display_dev)) {
        LOG_ERR("display device not ready, aborting test");
        return -1;
    }

    if (!device_is_ready(led_pwm)) {
        LOG_ERR("LED PWM %s device is not ready, aborting test", led_pwm->name);
        return -1;
    }

    display_handler();
    display_blanking_off(display_dev);

    led_set_brightness(led_pwm, 0, 100);

    LOG_INF("init complete.");
    return 0;
}

void display_handler() {
    k_mutex_lock(&display_handler_mutex, K_FOREVER);
    lv_task_handler();
    k_mutex_unlock(&display_handler_mutex);
}

lv_obj_t *show_qr_code(const char *url) {
    // cleanup old resources
    delete_qr_code();

    k_mutex_lock(&display_handler_mutex, K_FOREVER);
    qr = lv_qrcode_create(lv_scr_act(), 220, lv_color_black(), lv_color_white());
    lv_qrcode_update(qr, url, strlen(url));
    lv_obj_center(qr);
    lv_obj_set_style_border_color(qr, lv_color_white(), 0);
    lv_obj_set_style_border_width(qr, 5, 0);
    display_handler(); // also unlocks mutex

    return qr;
}

void delete_qr_code() {
    if (qr) {
        k_mutex_lock(&display_handler_mutex, K_FOREVER);
        lv_obj_del(qr);
        qr = NULL;
        display_handler(); // also unlocks mutex
    }
}

lv_obj_t *show_picture(const char *path) {
    LOG_INF("Showing picture: %s", path);

    // cleanup old resources
    delete_picture();

    k_mutex_lock(&display_handler_mutex, K_FOREVER);
    picture = lv_img_create(lv_scr_act());
    lv_img_set_src(picture, path);
    lv_obj_align(picture, LV_ALIGN_TOP_MID, 0, 0);
    display_handler(); // also unlocks mutex

    return picture;
}

void delete_picture() {
    if (picture) {
        k_mutex_lock(&display_handler_mutex, K_FOREVER);
        lv_obj_del(picture);
        picture = NULL;
        display_handler(); // also unlocks mutex
    }
}

/**
 * Set the display brightness.
 * @param v    brightness value between 0 (dark) to 100 (bright)
 */
void set_display_brightness(int v) {
    led_set_brightness(led_pwm, 0, MIN(MAX(v, 0), 100));
}
