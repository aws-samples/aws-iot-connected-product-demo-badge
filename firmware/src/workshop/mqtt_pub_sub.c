// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mqtt_pub_sub);

#include <lvgl.h>

#include "badge.h"

static const size_t expresslink_response_length = 512;
static char *expresslink_response = NULL;

static uint16_t d2c_count = 0;
static uint16_t c2d_count = 0;

static lv_obj_t *label_title = NULL;
#define LABEL_TITLE_X 0
#define LABEL_TITLE_Y 20
#define LABEL_TITLE_TEXT "-> MQTT Publish/Subscribe <-"

static lv_obj_t *label_d2c = NULL;
#define LABEL_D2C_X 10
#define LABEL_D2C_Y 50
#define LABEL_D2C_TEXT "Device to Cloud"

static lv_obj_t *label_d2c_cnt = NULL;
#define LABEL_D2C_CNT_X -10
#define LABEL_D2C_CNT_Y 50

static lv_obj_t *ta_d2c_msg = NULL;
#define TA_D2C_MSG_X 0
#define TA_D2C_MSG_Y 70
#define TA_D2C_MSG_SIZE_X 210
#define TA_D2C_MSG_SIZE_Y 60

static lv_obj_t *label_c2d = NULL;
#define LABEL_C2D_X 10
#define LABEL_C2D_Y 140
#define LABEL_C2D_TEXT "Cloud to Device"

static lv_obj_t *label_c2d_cnt = NULL;
#define LABEL_C2D_CNT_X -10
#define LABEL_C2D_CNT_Y 140

static lv_obj_t *ta_c2d_msg = NULL;
#define TA_C2D_MSG_X 0
#define TA_C2D_MSG_Y 160
#define TA_C2D_MSG_SIZE_X 210
#define TA_C2D_MSG_SIZE_Y 105

static void init_ui_display() {
    set_display_brightness(100);
    lv_obj_clean(lv_scr_act());

    label_title = lv_label_create(lv_scr_act());
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, LABEL_TITLE_X, LABEL_TITLE_Y);
    lv_obj_set_style_text_align(label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text_static(label_title, LABEL_TITLE_TEXT);

    label_d2c = lv_label_create(lv_scr_act());
    lv_obj_align(label_d2c, LV_ALIGN_TOP_LEFT, LABEL_D2C_X, LABEL_D2C_Y);
    lv_obj_set_style_text_align(label_d2c, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_d2c, LABEL_D2C_TEXT);

    label_d2c_cnt = lv_label_create(lv_scr_act());
    lv_obj_align(label_d2c_cnt, LV_ALIGN_TOP_RIGHT, LABEL_D2C_CNT_X, LABEL_D2C_CNT_Y);
    lv_obj_set_style_text_align(label_d2c_cnt, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text_fmt(label_d2c_cnt, "%d", d2c_count);

    label_c2d = lv_label_create(lv_scr_act());
    lv_obj_align(label_c2d, LV_ALIGN_TOP_LEFT, LABEL_C2D_X, LABEL_C2D_Y);
    lv_obj_set_style_text_align(label_c2d, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_c2d, LABEL_C2D_TEXT);

    label_c2d_cnt = lv_label_create(lv_scr_act());
    lv_obj_align(label_c2d_cnt, LV_ALIGN_TOP_RIGHT, LABEL_C2D_CNT_X, LABEL_C2D_CNT_Y);
    lv_obj_set_style_text_align(label_d2c_cnt, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text_fmt(label_c2d_cnt, "%d", c2d_count);

    ta_c2d_msg = lv_textarea_create(lv_scr_act());
    lv_obj_align(ta_c2d_msg, LV_ALIGN_TOP_MID, TA_C2D_MSG_X, TA_C2D_MSG_Y);
    lv_obj_set_size(ta_c2d_msg, TA_C2D_MSG_SIZE_X, TA_C2D_MSG_SIZE_Y);

    ta_d2c_msg = lv_textarea_create(lv_scr_act());
    lv_obj_align(ta_d2c_msg, LV_ALIGN_TOP_MID, TA_D2C_MSG_X, TA_D2C_MSG_Y);
    lv_obj_set_size(ta_d2c_msg, TA_D2C_MSG_SIZE_X, TA_D2C_MSG_SIZE_Y);

    display_handler();
}

static void update_d2c(uint8_t button, uint16_t cnt) {
    char text[50];
    snprintf(text, sizeof(text), "{\"event_type\":\"button_pressed\",\"value\":%d}", button);

    lv_textarea_set_text(ta_d2c_msg, text);
    lv_label_set_text_fmt(label_d2c_cnt, "%d", cnt);
    display_handler();
}

static void update_c2d(char *msg, uint16_t cnt) {
    lv_label_set_text_fmt(label_c2d_cnt, "%d", cnt);
    lv_textarea_set_text(ta_c2d_msg, expresslink_response);
    display_handler();
}

static void cleanup_ui_display() {
    if (label_title) {
        lv_obj_del(label_title);
        label_title = NULL;
    }
    if (label_d2c) {
        lv_obj_del(label_d2c);
        label_d2c = NULL;
    }
    if (label_d2c_cnt) {
        lv_obj_del(label_d2c_cnt);
        label_d2c_cnt = NULL;
    }
    if (label_c2d) {
        lv_obj_del(label_c2d);
        label_c2d = NULL;
    }
    if (label_c2d_cnt) {
        lv_obj_del(label_c2d_cnt);
        label_c2d_cnt = NULL;
    }
    if (ta_c2d_msg) {
        lv_obj_del(ta_c2d_msg);
        ta_c2d_msg = NULL;
    }
    if (ta_d2c_msg) {
        lv_obj_del(ta_d2c_msg);
        ta_d2c_msg = NULL;
    }
    display_handler();
}

void mqtt_pub_sub(void *context, void *dummy1, void *dummy2) {
    expresslink_response = k_malloc(expresslink_response_length);
    if (expresslink_response == NULL) {
        LOG_ERR("k_malloc for expresslink_response failed!");
        return;
    }

    init_ui_display();

    expresslink_reset();

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'MQTT Publish/Subscribe' module.");
            expresslink_reset();

            k_free(expresslink_response);
            expresslink_response = NULL;

            cleanup_ui_display();
            return;
        }

        display_handler();

        if (expresslink_check_event_pending()) {
            expresslink_send_command("AT+EVENT?\n", expresslink_response, expresslink_response_length);
            if (expresslink_is_event(expresslink_response, EL_EVENT_STARTUP)) {
                expresslink_send_command("AT+CONNECT!\n", NULL, 0);
            } else if (expresslink_is_event(expresslink_response, EL_EVENT_CONLOST)) {
                LOG_INF("CONLOST EVENT received! Reconnecting ...");
                expresslink_send_command("AT+CONNECT!\n", NULL, 0);
            } else if (expresslink_is_event(expresslink_response, EL_EVENT_CONNECT)) {
                int parameter = atoi(expresslink_response + 2);
                if (parameter == 0) {
                    LOG_INF("Successfully connected to AWS IoT Core!");
                    expresslink_send_command("AT+CONF Topic1=hello/badge\n", NULL, 0);
                    expresslink_send_command("AT+CONF Topic2=hello/cloud\n", NULL, 0);
                    expresslink_send_command("AT+CONF Topic3=hello/world\n", NULL, 0);
                    expresslink_send_command("AT+SUBSCRIBE2\n", NULL, 0);
                    expresslink_send_command("AT+SUBSCRIBE3\n", NULL, 0);
                    expresslink_send_command("AT+SEND1 {\"event_type\":\"connected\",\"value\":\"Fiat Lux! Welcome!\"}\n", NULL, 0);
                    LOG_INF("MQTT connection established and sent a Welcome message to the cloud!");
                } else {
                    LOG_INF("Connection attempt failed! Reconnecting...");
                    k_msleep(3000);
                    expresslink_send_command("AT+CONNECT!\n", NULL, 0);
                }
            } else if (expresslink_is_event(expresslink_response, EL_EVENT_MSG)) {
                bool success = expresslink_send_command("AT+GET\n", expresslink_response, expresslink_response_length);
                if (success && isdigit((int)expresslink_response[0])) {

                    c2d_count++;

                    LOG_INF("Received MQTT message on topic %s", expresslink_response);
                    size_t additional_lines = atoi(expresslink_response);
                    for (size_t i = 0; i < additional_lines; i++) {
                        expresslink_read_response_line(expresslink_response, expresslink_response_length);
                        LOG_INF("%s", expresslink_response);

                        update_c2d(expresslink_response, c2d_count);
                    }
                }
            } else if (expresslink_is_event(expresslink_response, EL_EVENT_SUBACK)) {
                // do nothing
            } else if (expresslink_is_event(expresslink_response, EL_EVENT_OTA)) {
                // do nothing
            } else {
                LOG_INF("Ignoring unhandled ExpressLink event: %s", expresslink_response);
            }
        }

        if (button1_pressed) {
            expresslink_send_command("AT+SEND1 {\"event_type\":\"button_pressed\",\"value\":1}\n", NULL, 0);
            k_msleep(50); // lazy debounce
            button1_pressed = false;

            d2c_count++;
            update_d2c(1, d2c_count);
        }
        if (button2_pressed) {
            expresslink_send_command("AT+SEND1 {\"event_type\":\"button_pressed\",\"value\":2}\n", NULL, 0);
            k_msleep(50); // lazy debounce
            button2_pressed = false;

            d2c_count++;
            update_d2c(2, d2c_count);
        }
        if (button3_pressed) {
            expresslink_send_command("AT+SEND1 {\"event_type\":\"button_pressed\",\"value\":3}\n", NULL, 0);
            k_msleep(50); // lazy debounce
            button3_pressed = false;

            d2c_count++;
            update_d2c(3, d2c_count);
        }
        if (button4_pressed) {
            expresslink_send_command("AT+SEND1 {\"event_type\":\"button_pressed\",\"value\":4}\n", NULL, 0);
            k_msleep(50); // lazy debounce
            button4_pressed = false;

            d2c_count++;
            update_d2c(4, d2c_count);
        }

        k_msleep(10);
    }
}
