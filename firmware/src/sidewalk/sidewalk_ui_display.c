// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#include "badge.h"

static lv_obj_t *label_sensor_data_title = NULL;
#define LABEL_TITLE_X 0
#define LABEL_TITLE_Y 20
#define LABEL_TITLE_TEXT "-> Sensor Data Ingestion <-\nover\n-> Amazon Sidewalk <-"

static lv_obj_t *label_temp = NULL;
#define LABEL_TEMP_X 20
#define LABEL_TEMP_Y 120
#define LABEL_TEMP_TEXT "Temperature:"

static lv_obj_t *label_temp_v = NULL;
#define LABEL_TEMP_V_X -20
#define LABEL_TEMP_V_Y 120

static lv_obj_t *label_hum = NULL;
#define LABEL_HUM_X 20
#define LABEL_HUM_Y 150
#define LABEL_HUM_TEXT "Humidity (relative):"

static lv_obj_t *label_hum_v = NULL;
#define LABEL_HUM_V_X -20
#define LABEL_HUM_V_Y 150

static lv_obj_t *label_light = NULL;
#define LABEL_LIGHT_X 20
#define LABEL_LIGHT_Y 180
#define LABEL_LIGHT_TEXT "Ambient Light:"

static lv_obj_t *label_light_v = NULL;
#define LABEL_LIGHT_V_X -20
#define LABEL_LIGHT_V_Y 180

lv_obj_t *sidewalk_label_last = NULL;
#define LABEL_LAST_X 0
#define LABEL_LAST_Y 250

void sidewalk_init_ui_display() {
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

    sidewalk_label_last = lv_label_create(lv_scr_act());
    lv_obj_align(sidewalk_label_last, LV_ALIGN_TOP_MID, LABEL_LAST_X, LABEL_LAST_Y);
    lv_obj_set_style_text_align(sidewalk_label_last, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text_static(sidewalk_label_last, "");

    display_handler();
}

void sidewalk_update_ui_display(float temperature, float humidity, int16_t light) {
    float fahrenheit = temperature * 9.0f/5.0f + 32.0f;
    lv_label_set_text_fmt(label_temp_v, "%.1f °C / %.0f °F", temperature, fahrenheit);
    lv_label_set_text_fmt(label_hum_v, "%.0f %%", humidity);
    lv_label_set_text_fmt(label_light_v, "%d units", light);

    display_handler();
}

void sidewalk_cleanup_ui_display() {
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
    if (sidewalk_label_last) {
        lv_obj_del(sidewalk_label_last);
        sidewalk_label_last = NULL;
    }
    display_handler();
}
