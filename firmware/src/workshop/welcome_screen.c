// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
LOG_MODULE_REGISTER(welcome_screen);

#include "app_version.h"
#include "badge.h"

static lv_obj_t *hello_world_label = NULL;
static lv_obj_t *ready_label = NULL;
static lv_obj_t *firmware_version_label = NULL;
static lv_obj_t *picture = NULL;
static lv_obj_t *qr_code = NULL;

static void init_ui_display() {
    set_display_brightness(100);
    lv_obj_clean(lv_scr_act());

    lv_obj_set_scrollbar_mode(lv_scr_act(), LV_SCROLLBAR_MODE_OFF);

    char text_buffer[128] = {0};

    hello_world_label = lv_label_create(lv_scr_act());
    lv_label_set_text(hello_world_label, "AWS re:Invent 2023\nDemo Badge\n\n\nWelcome! Hello World!");
    lv_obj_set_style_text_align(hello_world_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(hello_world_label, LV_ALIGN_TOP_MID, 0, 50);

    snprintf(text_buffer, sizeof(text_buffer), "Device firmware\n%s", APP_GIT_TS);
    firmware_version_label = lv_label_create(lv_scr_act());
    lv_label_set_text(firmware_version_label, text_buffer);
    lv_obj_set_style_text_align(firmware_version_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(firmware_version_label, LV_ALIGN_BOTTOM_MID, 0, -90);

    ready_label = lv_label_create(lv_scr_act());
    lv_label_set_text(ready_label, "Ready!");
    lv_obj_align(ready_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 10);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_line_rounded(&style_line, true);

    display_handler();
}

static void cleanup_ui_display() {
    if (hello_world_label) {
        lv_obj_del(hello_world_label);
        hello_world_label = NULL;
    }
    if (ready_label) {
        lv_obj_del(ready_label);
        ready_label = NULL;
    }
    if (firmware_version_label) {
        lv_obj_del(firmware_version_label);
        firmware_version_label = NULL;
    }
    if (picture) {
        delete_picture();
        picture = NULL;
    }
    if (qr_code) {
        delete_qr_code();
        qr_code = NULL;
    }
    display_handler();
}

void welcome_screen(void *context, void *dummy1, void *dummy2) {
    init_ui_display();

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'Welcome Screen' module.");
            cleanup_ui_display();
            return;
        }

        if (button1_pressed) {
            if (!qr_code) {
                qr_code = show_qr_code("https://github.com/aws-samples/aws-iot-connected-product-demo-badge");
            } else {
                delete_qr_code();
                qr_code = NULL;
            }
            k_msleep(50); // lazy debounce
            button1_pressed = false;
        }
        if (button3_pressed) {
            if (!picture) {
                picture = show_picture(USB_PATH("pictures/aws_logo.bmp"));
            } else {
                delete_picture();
                picture = NULL;
            }
            k_msleep(50); // lazy debounce
            button3_pressed = false;
        }
        if (button2_pressed || button4_pressed) {
            cleanup_ui_display();
            self_test(NULL, NULL, NULL);
            return;
        }

        k_msleep(10);
    }
}
