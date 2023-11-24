// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(device_location);

#include <lvgl.h>

#include "badge.h"

#include "core_json.h"

static char *expresslink_response = NULL;

static lv_obj_t *label_dl_title = NULL;
#define LABEL_DL_TITLE_X 0
#define LABEL_DL_TITLE_Y 20
#define LABEL_DL_TITLE_TEXT "-> Device Location <-"

static lv_obj_t *label_dl_b1 = NULL;
#define LABEL_DL_B1_X 10
#define LABEL_DL_B1_Y 50
#define LABEL_DL_B1_TEXT "--> Press any button to scan!"

static lv_obj_t *label_dl_net = NULL;
#define LABEL_DL_NET_X 10
#define LABEL_DL_NET_Y 90
#define LABEL_DL_NET_TEXT "Detected WiFi networks:"

static lv_obj_t *label_dl_net_v = NULL;
#define LABEL_DL_NET_V_X -10
#define LABEL_DL_NET_V_Y 90
#define LABEL_DL_NET_V_TEXT "-"

static lv_obj_t *label_dl_coords = NULL;
#define LABEL_DL_COORDS_X 10
#define LABEL_DL_COORDS_Y 150
#define LABEL_DL_COORDS_TEXT "Estimated location:"

static lv_obj_t *label_dl_coords_lat = NULL;
#define LABEL_DL_COORDS_LAT_X 10
#define LABEL_DL_COORDS_LAT_Y 180
#define LABEL_DL_COORDS_LAT_TEXT "Latitude:"

static lv_obj_t *label_dl_coords_long = NULL;
#define LABEL_DL_COORDS_LONG_X 10
#define LABEL_DL_COORDS_LONG_Y 200
#define LABEL_DL_COORDS_LONG_TEXT "Longitude:"

static void init_ui_display() {
    set_display_brightness(100);
    lv_obj_clean(lv_scr_act());

    label_dl_title = lv_label_create(lv_scr_act());
    lv_obj_align(label_dl_title, LV_ALIGN_TOP_MID, LABEL_DL_TITLE_X, LABEL_DL_TITLE_Y);
    lv_obj_set_style_text_align(label_dl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text_static(label_dl_title, LABEL_DL_TITLE_TEXT);

    label_dl_b1 = lv_label_create(lv_scr_act());
    lv_obj_align(label_dl_b1, LV_ALIGN_TOP_LEFT, LABEL_DL_B1_X, LABEL_DL_B1_Y);
    lv_obj_set_style_text_align(label_dl_b1, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_dl_b1, LABEL_DL_B1_TEXT);

    label_dl_net = lv_label_create(lv_scr_act());
    lv_obj_align(label_dl_net, LV_ALIGN_TOP_LEFT, LABEL_DL_NET_X, LABEL_DL_NET_Y);
    lv_obj_set_style_text_align(label_dl_net, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_dl_net, LABEL_DL_NET_TEXT);

    label_dl_net_v = lv_label_create(lv_scr_act());
    lv_obj_align(label_dl_net_v, LV_ALIGN_TOP_RIGHT, LABEL_DL_NET_V_X, LABEL_DL_NET_V_Y);
    lv_obj_set_style_text_align(label_dl_net_v, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text_static(label_dl_net_v, LABEL_DL_NET_V_TEXT);

    label_dl_coords = lv_label_create(lv_scr_act());
    lv_obj_align(label_dl_coords, LV_ALIGN_TOP_LEFT, LABEL_DL_COORDS_X, LABEL_DL_COORDS_Y);
    lv_obj_set_style_text_align(label_dl_coords, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_dl_coords, LABEL_DL_COORDS_TEXT);

    label_dl_coords_lat = lv_label_create(lv_scr_act());
    lv_obj_align(label_dl_coords_lat, LV_ALIGN_TOP_LEFT, LABEL_DL_COORDS_LAT_X, LABEL_DL_COORDS_LAT_Y);
    lv_obj_set_style_text_align(label_dl_coords_lat, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_dl_coords_lat, LABEL_DL_COORDS_LAT_TEXT " (not yet known)");

    label_dl_coords_long = lv_label_create(lv_scr_act());
    lv_obj_align(label_dl_coords_long, LV_ALIGN_TOP_LEFT, LABEL_DL_COORDS_LONG_X, LABEL_DL_COORDS_LONG_Y);
    lv_obj_set_style_text_align(label_dl_coords_long, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text_static(label_dl_coords_long, LABEL_DL_COORDS_LONG_TEXT " (not yet known)");

    display_handler();
}

static bool device_location_extract_coords(const char *response, const char *query, size_t queryLength, char *outBuffer, size_t outBufferLen) {
    bool coords_found = false;

    JSONStatus_t result;

    // LOG_INF( "Validating: %s\n", response);

    // Calling JSON_Validate() is not necessary if the document is guaranteed to be valid.
    result = JSON_Validate(response, strlen(response));

    if (result == JSONSuccess) {
        char *coords_substr;
        size_t value_length;

        result = JSON_Search((char*)response, strlen(response), query, queryLength, &coords_substr, &value_length);

        if (result == JSONSuccess) {
            // The pointer "value" will point to a location in the "buffer".
            char save = coords_substr[value_length];

            // After saving the character, set it to a null byte for printing.
            coords_substr[value_length] = '\0';

            snprintf(outBuffer, outBufferLen, "%s", coords_substr);

            // Restore the original character.
            coords_substr[value_length] = save;
            coords_found = true;
        } else {
            LOG_INF("It's a JSON but no '%s' were found.", query);
            snprintf(outBuffer, outBufferLen, "%s", "not found");

            coords_found = false;
        }
    } else {
        LOG_INF("Validation ERROR. Is it a JSON string?");
        snprintf(outBuffer, outBufferLen, "%s", "error");

        coords_found = false;
    }
    return coords_found;
}

static size_t device_location_count_networks(const char *scan_result) {
    const char token[] = "MacAddress";
    size_t token_len = strlen(token);
    size_t count = 0;
    size_t i = 0;

    size_t length = strlen(scan_result) - token_len;
    while (i < length) {
        if (strncmp(&scan_result[i], token, token_len) == 0) {
            count++;
            i += token_len;
        } else {
            i += 1;
        }
    }

    return count;
}

static void update_ui_display(const char *response) {
    // enough space for: '-' + 3 digits + '.' + 16 decimal digits + null-byte
    char coords_latitude[25], coords_longitude[25];

    const char query_longitude[] = "coordinates[0]";
    const char query_latitude[] = "coordinates[1]";

    device_location_extract_coords(response, query_latitude, sizeof(query_latitude) - 1, coords_latitude, sizeof(coords_latitude) - 1);
    device_location_extract_coords(response, query_longitude, sizeof(query_longitude) - 1, coords_longitude, sizeof(coords_longitude) - 1);

    LOG_INF("Latitude: %s", coords_latitude);
    LOG_INF("Longitude: %s", coords_longitude);

    lv_label_set_text_fmt(label_dl_coords_lat, "%s %s", LABEL_DL_COORDS_LAT_TEXT, coords_latitude);
    lv_label_set_text_fmt(label_dl_coords_long, "%s %s", LABEL_DL_COORDS_LONG_TEXT, coords_longitude);
    display_handler();
}

static void cleanup_ui_display() {
    if (label_dl_title) {
        lv_obj_del(label_dl_title);
        label_dl_title = NULL;
    }
    if (label_dl_b1) {
        lv_obj_del(label_dl_b1);
        label_dl_b1 = NULL;
    }
    if (label_dl_net) {
        lv_obj_del(label_dl_net);
        label_dl_net = NULL;
    }
    if (label_dl_net_v) {
        lv_obj_del(label_dl_net_v);
        label_dl_net_v = NULL;
    }
    if (label_dl_coords) {
        lv_obj_del(label_dl_coords);
        label_dl_coords = NULL;
    }
    if (label_dl_coords_lat) {
        lv_obj_del(label_dl_coords_lat);
        label_dl_coords_lat = NULL;
    }
    if (label_dl_coords_long) {
        lv_obj_del(label_dl_coords_long);
        label_dl_coords_long = NULL;
    }
}

void device_location(void *context, void *dummy1, void *dummy2) {
    size_t cmd_length = 4096;
    char *cmd = k_malloc(cmd_length);
    if (cmd == NULL) {
        LOG_ERR("k_malloc for cmd failed!");
        return;
    }

    size_t expresslink_response_length = 4096; // many WiFi networks might produce a large scan response
    expresslink_response = k_malloc(expresslink_response_length);
    if (expresslink_response == NULL) {
        LOG_ERR("k_malloc for expresslink_response failed!");
        return;
    }

    init_ui_display();

    expresslink_reset();

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'Device Location' module.");
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

                    char thing_name[64];
                    expresslink_send_command("AT+CONF? ThingName\n", thing_name, sizeof(thing_name));

                    snprintf(cmd, cmd_length, "AT+CONF Topic1=$aws/device_location/%s/get_position_estimate\n", thing_name);
                    expresslink_send_command(cmd, expresslink_response, expresslink_response_length);
                    snprintf(cmd, cmd_length, "AT+CONF Topic2=$aws/device_location/%s/get_position_estimate/accepted\n", thing_name);
                    expresslink_send_command(cmd, expresslink_response, expresslink_response_length);
                    snprintf(cmd, cmd_length, "AT+CONF Topic3=$aws/device_location/%s/get_position_estimate/rejected\n", thing_name);
                    expresslink_send_command(cmd, expresslink_response, expresslink_response_length);

                    expresslink_send_command("AT+SUBSCRIBE2\n", NULL, 0);
                    expresslink_send_command("AT+SUBSCRIBE3\n", NULL, 0);
                } else {
                    LOG_INF("Connection attempt failed! Reconnecting...");
                    k_msleep(3000);
                    expresslink_send_command("AT+CONNECT!\n", NULL, 0);
                }
            } else if (expresslink_is_event(expresslink_response, EL_EVENT_MSG)) {
                bool success = expresslink_send_command("AT+GET\n", expresslink_response, expresslink_response_length);
                if (success && isdigit((int)expresslink_response[0])) {
                    LOG_INF("Received MQTT message: %s", expresslink_response);
                    size_t additional_lines = atoi(expresslink_response);
                    for (size_t i = 0; i < additional_lines; i++) {
                        expresslink_read_response_line(expresslink_response, expresslink_response_length);
                        LOG_INF("%s", expresslink_response);
                        update_ui_display(expresslink_response);
                    }
                } else {
                    LOG_WRN("AT+GET failed! %d %s", success, expresslink_response);
                }
            } else if (expresslink_is_event(expresslink_response, EL_EVENT_SUBACK)) {
                // do nothing
            } else {
                LOG_INF("Ignoring unhandled ExpressLink event: %s", expresslink_response);
            }
        }

        if (button1_pressed || button2_pressed || button3_pressed || button4_pressed) {
            lv_label_set_text_fmt(label_dl_net_v, "wait");
            lv_label_set_text_fmt(label_dl_coords_lat, "%s %s", LABEL_DL_COORDS_LAT_TEXT, "-");
            lv_label_set_text_fmt(label_dl_coords_long, "%s %s", LABEL_DL_COORDS_LONG_TEXT, "-");
            display_handler();

            expresslink_send_command("AT+DIAG WIFI SCAN workshop MacAddress Rss\n", expresslink_response, expresslink_response_length);

            // WARNING: do not log response output - it might be too large and crash the logging subsystem

            size_t num_networks = device_location_count_networks(expresslink_response);
            lv_label_set_text_fmt(label_dl_net_v, "%d", num_networks);
            display_handler();

            // truncate too many scan results
            // {"WiFiAccessPoints":[{"MacAddress":"ab:cd:ef:12:34:56","Rss":-50}]}
            const size_t max_len = 2048;
            size_t len = strlen(expresslink_response);
            if (len > max_len) {
                char *pos = strstr(expresslink_response + max_len - 50, "},{");
                strcpy(pos, "}]}");
            }

            snprintf(cmd, cmd_length, "AT+SEND1 %s\n", expresslink_response);
            LOG_INF("Found %d WiFi networks.", num_networks);

            expresslink_send_command(cmd, NULL, 0);

            k_msleep(50); // lazy debounce
            button1_pressed = false;
            button2_pressed = false;
            button3_pressed = false;
            button4_pressed = false;
        }

        k_msleep(10);
    }
}
