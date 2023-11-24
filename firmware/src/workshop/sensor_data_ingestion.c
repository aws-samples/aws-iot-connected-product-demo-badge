// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <string.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/base64.h>
LOG_MODULE_REGISTER(sensor_data_ingestion);

#include "badge.h"

#include <lvgl.h>

static char *expresslink_response = NULL;

static lv_obj_t *label_sensor_data_title = NULL;
#define LABEL_TITLE_X 0
#define LABEL_TITLE_Y 20
#define LABEL_TITLE_TEXT "-> Sensor Data Ingestion <-"

static lv_obj_t *label_temp = NULL;
#define LABEL_TEMP_X 20
#define LABEL_TEMP_Y 80
#define LABEL_TEMP_TEXT "Temperature:"

static lv_obj_t *label_temp_v = NULL;
#define LABEL_TEMP_V_X -20
#define LABEL_TEMP_V_Y 80

static lv_obj_t *label_hum = NULL;
#define LABEL_HUM_X 20
#define LABEL_HUM_Y 110
#define LABEL_HUM_TEXT "Humidity (relative):"

static lv_obj_t *label_hum_v = NULL;
#define LABEL_HUM_V_X -20
#define LABEL_HUM_V_Y 110

static lv_obj_t *label_light = NULL;
#define LABEL_LIGHT_X 20
#define LABEL_LIGHT_Y 140
#define LABEL_LIGHT_TEXT "Ambient Light:"

static lv_obj_t *label_light_v = NULL;
#define LABEL_LIGHT_V_X -20
#define LABEL_LIGHT_V_Y 140

static lv_obj_t *label_last = NULL;
#define LABEL_LAST_X 0
#define LABEL_LAST_Y 250

static void init_ui_display() {
    set_display_brightness(100);
    lv_obj_clean(lv_scr_act());

    label_sensor_data_title = lv_label_create(lv_scr_act());
    lv_obj_align(label_sensor_data_title, LV_ALIGN_TOP_MID, LABEL_TITLE_X, LABEL_TITLE_Y);
    lv_obj_set_style_text_align(label_sensor_data_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text_static(label_sensor_data_title, LABEL_TITLE_TEXT);

    label_temp = lv_label_create(lv_scr_act());
    lv_obj_align(label_temp, LV_ALIGN_TOP_LEFT, LABEL_TEMP_X, LABEL_TEMP_Y);
    lv_obj_set_style_text_align(label_temp, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_temp, LABEL_TEMP_TEXT);

    label_temp_v = lv_label_create(lv_scr_act());
    lv_obj_align(label_temp_v, LV_ALIGN_TOP_RIGHT, LABEL_TEMP_V_X, LABEL_TEMP_V_Y);
    lv_obj_set_style_text_align(label_temp_v, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text_static(label_temp_v, "-");

    label_hum = lv_label_create(lv_scr_act());
    lv_obj_align(label_hum, LV_ALIGN_TOP_LEFT, LABEL_HUM_X, LABEL_HUM_Y);
    lv_obj_set_style_text_align(label_hum, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_hum, LABEL_HUM_TEXT);

    label_hum_v = lv_label_create(lv_scr_act());
    lv_obj_align(label_hum_v, LV_ALIGN_TOP_RIGHT, LABEL_HUM_V_X, LABEL_HUM_V_Y);
    lv_obj_set_style_text_align(label_hum_v, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text_static(label_hum_v, "-");

    label_light = lv_label_create(lv_scr_act());
    lv_obj_align(label_light, LV_ALIGN_TOP_LEFT, LABEL_LIGHT_X, LABEL_LIGHT_Y);
    lv_obj_set_style_text_align(label_light, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_light, LABEL_LIGHT_TEXT);

    label_light_v = lv_label_create(lv_scr_act());
    lv_obj_align(label_light_v, LV_ALIGN_TOP_RIGHT, LABEL_LIGHT_V_X, LABEL_LIGHT_V_Y);
    lv_obj_set_style_text_align(label_light_v, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text_static(label_light_v, "-");

    label_last = lv_label_create(lv_scr_act());
    lv_obj_align(label_last, LV_ALIGN_TOP_MID, LABEL_LAST_X, LABEL_LAST_Y);
    lv_obj_set_style_text_align(label_last, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text_static(label_last, "");

    display_handler();
}

static void update_ui_display(float temperature, float humidity, int16_t light) {
    float fahrenheit = temperature * 9.0f/5.0f + 32.0f;
    lv_label_set_text_fmt(label_temp_v, "%.1f °C / %.0f F", temperature, fahrenheit);
    lv_label_set_text_fmt(label_hum_v, "%.0f %%", humidity);
    lv_label_set_text_fmt(label_light_v, "%d units", light);

    display_handler();
}

static void cleanup_ui_display() {
    if (label_sensor_data_title) {
        lv_obj_del(label_sensor_data_title);
        label_sensor_data_title = NULL;
    }
    if (label_temp) {
        lv_obj_del(label_temp);
        label_temp = NULL;
    }
    if (label_temp_v) {
        lv_obj_del(label_temp_v);
        label_temp_v = NULL;
    }
    if (label_hum) {
        lv_obj_del(label_hum);
        label_hum = NULL;
    }
    if (label_hum_v) {
        lv_obj_del(label_hum_v);
        label_hum_v = NULL;
    }
    if (label_light) {
        lv_obj_del(label_light);
        label_light = NULL;
    }
    if (label_light_v) {
        lv_obj_del(label_light_v);
        label_light_v = NULL;
    }
    if (label_last) {
        lv_obj_del(label_last);
        label_last = NULL;
    }
    display_handler();
}

void sensor_data_ingestion(void *p1, void *p2, void *p3) {
    int32_t update_rate = (int32_t)p1;
    if (update_rate == 0) {
        update_rate = 3000;
    }

    LOG_INF("starting with update rate of %d ms...", update_rate);

    // max size: 'OK ' + an error message
    const size_t expresslink_response_length = 128;
    expresslink_response = k_malloc(expresslink_response_length);
    if (expresslink_response == NULL) {
        LOG_ERR("k_malloc failed!");
    }

    size_t cmd_length = 128;
    char *cmd = k_malloc(cmd_length);
    if (cmd == NULL) {
        LOG_ERR("k_malloc failed!");
    }

    init_ui_display();

    expresslink_reset();

    bool connected = false;

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'Sensor Data Ingestion' module.");
            expresslink_reset();

            k_free(expresslink_response);
            expresslink_response = NULL;

            k_free(cmd);
            cmd = NULL;

            cleanup_ui_display();

            return;
        }

        if (expresslink_check_event_pending()) {
            expresslink_send_command("AT+EVENT?\n", expresslink_response, expresslink_response_length);
            if (expresslink_is_event(expresslink_response, EL_EVENT_STARTUP)) {
                expresslink_send_command("AT+CONNECT!\n", NULL, 0);
            } else if (expresslink_is_event(expresslink_response, EL_EVENT_CONLOST)) {
                connected = false;
                LOG_INF("CONLOST EVENT received! Reconnecting ...");
                expresslink_send_command("AT+CONNECT!\n", NULL, 0);
            } else if (expresslink_is_event(expresslink_response, EL_EVENT_CONNECT)) {
                int parameter = atoi(expresslink_response + 2);
                if (parameter == 0) {
                    LOG_INF("Successfully connected to AWS IoT Core!");
                    expresslink_send_command("AT+CONF Topic1=$aws/rules/demo_badge_sensors\n", NULL, 0);
                    connected = true;
                } else {
                    connected = false;
                    LOG_INF("Connection attempt failed! Reconnecting...");
                    k_msleep(3000);
                    expresslink_send_command("AT+CONNECT!\n", NULL, 0);
                }
            }
        }

        if (connected) {
            sht3xd_sample v;
            read_sht31_sample(&v);
            int16_t light = read_ambient_light();

            snprintf(cmd,
                    cmd_length,
                    "AT+SEND1 {\"data\":{\"temperature\":%.1f,\"humidity\":%.1f,\"light\":%d,\"source\":\"mqtt\"}}\n",
                    v.temperature,
                    v.humidity,
                    light);

            update_ui_display(
                v.temperature,
                v.humidity,
                light);

            LOG_INF("Sending updated sensor data...");
            expresslink_send_command(cmd, NULL, 0);
        }

        const int64_t last_updated_at = k_uptime_get();
        const int64_t deadline = last_updated_at + update_rate;
        while (k_uptime_get() < deadline) {
            lv_label_set_text_fmt(label_last, "last updated %.1fs ago...", MAX(0, k_uptime_get() - last_updated_at) / 1000.0f);
            display_handler();
            k_msleep(100);
        }
    }
}
