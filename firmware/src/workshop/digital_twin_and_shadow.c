// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(digital_twin_and_shadow);

#include <lvgl.h>

#include "core_json.h"

#include "badge.h"

bool user_led_blinking = false;
bool send_sensor_data = false;

static const size_t expresslink_response_length = 4096;
static char *expresslink_response = NULL;
static bool expresslink_connected = false;

#define display_state_length (128)
static char display_state[display_state_length];

static uint32_t button1_last_pressed = 0;
static uint32_t button2_last_pressed = 0;
static uint32_t button3_last_pressed = 0;
static uint32_t button4_last_pressed = 0;

void set_led_color(uint8_t index, const char *value) {
    uint32_t new_color = atoi(value);
    uint8_t r = (new_color & 0xFF0000) >> 16;
    uint8_t g = (new_color & 0x00FF00) >> 8;
    uint8_t b = new_color & 0x0000FF;
    led_strip_set_pixel(index, r, g, b);
}

void report_shadow_change(const char *key, const char *value, bool quote) {
    char cmd[300];
    snprintf(cmd,
             sizeof(cmd),
             "AT+SHADOW UPDATE {\"state\":{\"desired\":{\"%s\":null},\"reported\":{\"%s\":%s%s%s" /* quote value quote */ "}}}\n",
             key,
             key,
             quote ? "\"" : "",
             value,
             quote ? "\"" : "");
    expresslink_send_command(cmd, NULL, 0);
}

void parse_shadow_state(char *state, size_t state_length, bool update_shadow_after_processing) {
    char *value;
    size_t value_length;

    char *query = "user_led";
    JSONStatus_t result = JSON_Search(state, state_length, query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        char tmp = value[value_length];
        value[value_length] = 0; // set a 0-byte to terminate string
        if (strcmp(value, "off") == 0) {
            user_led_blinking = false;
            turn_user_led_off();
        } else if (strcmp(value, "blinking") == 0) {
            user_led_blinking = true;
        } else if (strcmp(value, "on") == 0) {
            user_led_blinking = false;
            turn_user_led_on();
        }
        if (update_shadow_after_processing) {
            report_shadow_change(query, value, true);
        }
        value[value_length] = tmp; // restore character
    }

    query = "led_brightness";
    result = JSON_Search(state, state_length, query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        char tmp = value[value_length];
        value[value_length] = 0; // set a 0-byte to terminate string
        uint8_t new_brightness = atoi(value);
        led_strip_set_brightness(new_brightness);
        if (update_shadow_after_processing) {
            report_shadow_change(query, value, false);
        }
        value[value_length] = tmp; // restore character
    }
    query = "led_1";
    result = JSON_Search(state, state_length, query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        char tmp = value[value_length];
        value[value_length] = 0; // set a 0-byte to terminate string
        set_led_color(0, value);
        if (update_shadow_after_processing) {
            report_shadow_change(query, value, false);
        }
        value[value_length] = tmp; // restore character
    }
    query = "led_2";
    result = JSON_Search(state, state_length, query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        char tmp = value[value_length];
        value[value_length] = 0; // set a 0-byte to terminate string
        set_led_color(1, value);
        if (update_shadow_after_processing) {
            report_shadow_change(query, value, false);
        }
        value[value_length] = tmp; // restore character
    }
    query = "led_3";
    result = JSON_Search(state, state_length, query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        char tmp = value[value_length];
        value[value_length] = 0; // set a 0-byte to terminate string
        set_led_color(2, value);
        if (update_shadow_after_processing) {
            report_shadow_change(query, value, false);
        }
        value[value_length] = tmp; // restore character
    }

    query = "send_sensor_data";
    result = JSON_Search(state, state_length, query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        char tmp = value[value_length];
        value[value_length] = 0; // set a 0-byte to terminate string
        if (strcmp(value, "true") == 0) {
            send_sensor_data = true;
        } else {
            send_sensor_data = false;
        }
        if (update_shadow_after_processing) {
            report_shadow_change(query, send_sensor_data ? "true" : "false", false);
        }
        value[value_length] = tmp; // restore character
    }

    query = "qr_code";
    result = JSON_Search(state, state_length, query, strlen(query), &value, &value_length);
    if (result == JSONSuccess && value_length > 3 && value_length < 200) {
        report_shadow_change("picture", "none", true);

        char tmp = value[value_length];
        value[value_length] = 0; // set a 0-byte to terminate string
        delete_picture();
        show_qr_code(value);
        snprintf(display_state, display_state_length, "%s", value);
        if (update_shadow_after_processing) {
            report_shadow_change(query, value, true);
        }
        value[value_length] = tmp; // restore character
    }

    query = "picture";
    result = JSON_Search(state, state_length, query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        report_shadow_change("qr_code", "null", false);

        char tmp = value[value_length];
        value[value_length] = 0; // set a 0-byte to terminate string

        if (strcmp(value, "none") == 0) {
            delete_picture();
        } else if (value_length < 20) {
            char path[40];
            char fmt[] = USB_PATH("pictures/%.*s.bmp");
            snprintf(path, sizeof(path), fmt, value_length, value);

            if (strcmp(display_state, path) != 0) {
                // prevent re-drawing of the same picture multiple times
                delete_qr_code();
                show_picture(path);
                snprintf(display_state, display_state_length, "%s", path);
            }
        }

        if (update_shadow_after_processing) {
            report_shadow_change(query, value, true);
        }
        value[value_length] = tmp; // restore character
    }

    query = "display_brightness";
    result = JSON_Search(state, state_length, query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        char tmp = value[value_length];
        value[value_length] = 0; // set a 0-byte to terminate string
        int v = atoi(value);
        set_display_brightness(v);
        if (update_shadow_after_processing) {
            report_shadow_change(query, value, false);
        }
        value[value_length] = tmp; // restore character
    }
}

void handle_shadow_doc(char *doc) {
    if (strncmp(doc, "1 ", 2) == 0) {
        // shadow doc was accepted, just cut of the prefix
        doc = &doc[2];
    } else if (strncmp(doc, "0 ", 2) == 0) {
        LOG_WRN("SHADOW rejected!");
        return;
    }

    char *value;
    size_t value_length;

    // first: process existing reported state without reporting them back to the cloud
    char *query = "state.reported";
    JSONStatus_t result = JSON_Search(doc, strlen(doc), query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        LOG_INF("Parsing shadow %s...", query);
        parse_shadow_state(value, value_length, false);
    }

    // second: process delta update state
    query = "state";
    result = JSON_Search(doc, strlen(doc), query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        LOG_INF("Parsing shadow %s...", query);
        parse_shadow_state(value, value_length, true);
    }

    // last: process new desired state
    query = "state.desired";
    result = JSON_Search(doc, strlen(doc), query, strlen(query), &value, &value_length);
    if (result == JSONSuccess) {
        LOG_INF("Parsing shadow %s...", query);
        parse_shadow_state(value, value_length, true);
    }
}

void handle_expresslink_event() {
    expresslink_send_command("AT+EVENT?\n", expresslink_response, expresslink_response_length);
    if (expresslink_is_event(expresslink_response, EL_EVENT_STARTUP)) {
        expresslink_send_command("AT+CONNECT!\n", NULL, 0);
    } else if (expresslink_is_event(expresslink_response, EL_EVENT_CONLOST)) {
        expresslink_connected = false;
        LOG_INF("CONLOST EVENT received! Reconnecting...");
        expresslink_send_command("AT+CONNECT!\n", NULL, 0);
    } else if (expresslink_is_event(expresslink_response, EL_EVENT_CONNECT)) {
        int parameter = atoi(expresslink_response + 2);
        if (parameter == 0) {
            LOG_INF("Successfully connected to AWS IoT Core!");
            expresslink_send_command("AT+CONF Topic1=demo_badge/sensors\n", NULL, 0);
            expresslink_send_command("AT+CONF EnableShadow=1\n", NULL, 0);
            expresslink_send_command("AT+SHADOW INIT\n", NULL, 0);
            expresslink_connected = true;
        } else {
            LOG_INF("Connection attempt failed! Reconnecting...");
            k_msleep(3000);
            expresslink_send_command("AT+CONNECT!\n", NULL, 0);
        }
    } else if (expresslink_is_event(expresslink_response, EL_EVENT_SHADOW_DOC)) {
        expresslink_send_command("AT+SHADOW GET DOC\n", expresslink_response, expresslink_response_length);
        handle_shadow_doc(expresslink_response);
    } else if (expresslink_is_event(expresslink_response, EL_EVENT_SHADOW_DELTA)) {
        expresslink_send_command("AT+SHADOW GET DELTA\n", expresslink_response, expresslink_response_length);
        handle_shadow_doc(expresslink_response);
    } else if (expresslink_is_event(expresslink_response, EL_EVENT_SHADOW_UPDATE)) {
        expresslink_send_command("AT+SHADOW GET UPDATE\n", expresslink_response, expresslink_response_length);
        // shadow update accepted, no further processing needed
    } else if (expresslink_is_event(expresslink_response, EL_EVENT_SHADOW_SUBACK)) {
        expresslink_send_command("AT+SHADOW DOC\n", NULL, 0);
    } else if (expresslink_is_event(expresslink_response, EL_EVENT_SHADOW_INIT)) {
        expresslink_send_command("AT+SHADOW SUBSCRIBE\n", NULL, 0);
        expresslink_send_command("AT+SHADOW UPDATE {\"state\":{\"reported\":{\"button_1\":0,\"button_2\":0,\"button_3\":0,\"button_4\":0}}}\n", NULL, 0);
    } else if (expresslink_is_event(expresslink_response, EL_EVENT_SHADOW_INIT_FAILED)) {
        LOG_WRN("shadow init failed!");
    } else {
        LOG_INF("Ignoring unhandled ExpressLink event: %s", expresslink_response);
    }
}

void report_data() {
    if (!expresslink_connected) {
        return;
    }

    sht3xd_sample sht3xd_v;
    read_sht31_sample(&sht3xd_v);

    lsm6dsl_sample lsm6dsl_v;
    read_lsm6dsl_sample(&lsm6dsl_v);

    int16_t light_v = read_ambient_light();

    char json[256];
    snprintf(json,
             sizeof(json),
             "AT+SEND1 {\"temperature\":%.1f,\"humidity\":%.1f,\"light\":%d,\"acceleration_x\":%.1f,\"acceleration_y\":%.1f,\"acceleration_z\":%.1f,\"angular_velocity_x\":%.3f,\"angular_velocity_y\":%.3f,\"angular_velocity_z\":%.3f}\n",
             sht3xd_v.temperature,
             sht3xd_v.humidity,
             light_v,
             lsm6dsl_v.accel_x,
             lsm6dsl_v.accel_y,
             lsm6dsl_v.accel_z,
             lsm6dsl_v.angular_velocity_x,
             lsm6dsl_v.angular_velocity_y,
             lsm6dsl_v.angular_velocity_z);
    expresslink_send_command(json, NULL, 0);
}

static void init_ui_display() {
    set_display_brightness(100);
    lv_obj_clean(lv_scr_act());
}

static void cleanup_ui_display() {
    delete_qr_code();
    delete_picture();
    display_handler();
}

void digital_twin_and_shadow(void *context, void *dummy1, void *dummy2) {
    expresslink_response = k_malloc(expresslink_response_length);
    if (expresslink_response == NULL) {
        LOG_ERR("k_malloc failed!");
        return;
    }

    init_ui_display();

    expresslink_reset();

    int64_t last_update_time = k_uptime_get();
    int64_t last_user_led_blink_time = k_uptime_get();

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'Digital Twin and Shadow' module.");
            expresslink_reset();

            k_free(expresslink_response);
            expresslink_response = NULL;

            cleanup_ui_display();

            led_strip_set_pixel(0, 0, 0, 0);
            k_msleep(100);
            led_strip_set_pixel(1, 0, 0, 0);
            k_msleep(100);
            led_strip_set_pixel(2, 0, 0, 0);
            k_msleep(100);
            return;
        }

        display_handler();

        if (expresslink_check_event_pending()) {
            handle_expresslink_event();
        }

        if (send_sensor_data && k_uptime_get() > last_update_time + 100) {
            last_update_time = k_uptime_get();
            report_data();
        }

        if (user_led_blinking && k_uptime_get() > last_user_led_blink_time + 250) {
            toggle_user_led();
            last_user_led_blink_time = k_uptime_get();
        }

        char json[80];
        if (button1_pressed) {
            button1_last_pressed = (uint32_t)(k_uptime_get() / 1000);
            snprintf(json, sizeof(json), "AT+SHADOW UPDATE {\"state\":{\"reported\":{\"button_1\":%u}}}\n", button1_last_pressed);
            expresslink_send_command(json, NULL, 0);
            k_msleep(50); // lazy debounce
            button1_pressed = false;
        }
        if (button2_pressed) {
            button2_last_pressed = (uint32_t)(k_uptime_get() / 1000);
            snprintf(json, sizeof(json), "AT+SHADOW UPDATE {\"state\":{\"reported\":{\"button_2\":%u}}}\n", button2_last_pressed);
            expresslink_send_command(json, NULL, 0);
            k_msleep(50); // lazy debounce
            button2_pressed = false;
        }
        if (button3_pressed) {
            button3_last_pressed = (uint32_t)(k_uptime_get() / 1000);
            snprintf(json, sizeof(json), "AT+SHADOW UPDATE {\"state\":{\"reported\":{\"button_3\":%u}}}\n", button3_last_pressed);
            expresslink_send_command(json, NULL, 0);
            k_msleep(50); // lazy debounce
            button3_pressed = false;
        }
        if (button4_pressed) {
            button4_last_pressed = (uint32_t)(k_uptime_get() / 1000);
            snprintf(json, sizeof(json), "AT+SHADOW UPDATE {\"state\":{\"reported\":{\"button_4\":%u}}}\n", button4_last_pressed);
            expresslink_send_command(json, NULL, 0);
            k_msleep(50); // lazy debounce
            button4_pressed = false;
        }

        k_msleep(10);
    }
}
