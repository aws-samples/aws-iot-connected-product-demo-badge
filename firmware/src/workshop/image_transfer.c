// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(image_transfer);

#include <lvgl.h>

#include "badge.h"

#define IMAGE_HEIGHT (240U)
#define IMAGE_WIDTH (240U)
#define IMG_DATA_SIZE (sizeof(uint16_t) * IMAGE_WIDTH * IMAGE_HEIGHT)
#define ROWS_TO_BUFFER (2U)
#define ROW_SIZE (IMAGE_WIDTH * 2U) // one full row of pixels, allows for line-by-line rendering
#define BLOCK_SIZE (ROW_SIZE * ROWS_TO_BUFFER)

#define TRANSFERRED_IMAGE_PATH USB_PATH("transferred_image.bin")

static lv_obj_t *preload = NULL;
static lv_obj_t *picture = NULL;
static lv_obj_t *progress_label = NULL;

// data payload is hex encoded = 2 hex digits for one payload byte
// max size: 'OK ' + length + data[BLOCK_SIZE*2] + checksum
static const size_t expresslink_response_length = 64 + BLOCK_SIZE * 2;
static char *expresslink_response = NULL;

size_t render_row(size_t y) {
    char cmd[48];

    snprintf(cmd, sizeof(cmd), "AT+OTA SEEK %u\n", (uint32_t)(y * ROW_SIZE));
    expresslink_send_command(cmd, expresslink_response, expresslink_response_length);

    snprintf(cmd, sizeof(cmd), "AT+OTA READ %u\n", BLOCK_SIZE);
    expresslink_send_command(cmd, expresslink_response, expresslink_response_length);

    // find the space character that separates bytes-count prefix from data
    char *start = strchr(expresslink_response, ' ') + 1;
    // ignore 4-hex-digit checksum at the end of the line

    struct fs_file_t file;
    fs_file_t_init(&file);
    int ret = fs_open(&file, TRANSFERRED_IMAGE_PATH, FS_O_WRITE);
    if (ret != 0) {
        LOG_ERR("fs_open failed: %d", ret);
    }
    ret = fs_seek(&file, sizeof(lv_img_header_t) + y * ROW_SIZE, FS_SEEK_SET); // lv_img_header_t = 4-byte file header for LVGL
    if (ret != 0) {
        LOG_ERR("fs_seek failed: %d", ret);
    }

    // we retrieved one row of pixel data, one row is IMAGE_WIDTH pixels,
    // each pixel is 16 bit or 2 bytes, over serial, they come in as hex-encoded,
    // 2 bytes = 4 hex digits = 4 characters to read,
    // decode two hex digits to one byte, then repeat for a second byte === one pixel with 16 bit
    // pixel data in RGB565 bits across two bytes: RRRRRGGG GGGBBBBB
    for (size_t r = 0; r < ROWS_TO_BUFFER; r++) {
        LOG_INF("rendering row %d...", y + r);
        for (size_t x = 0; x < IMAGE_WIDTH; x++) {
            size_t i = r * ROW_SIZE * 2 + x * 4;
            uint16_t v = (HEX_TO_BYTE(start[i], start[i + 1]) << 8) + (HEX_TO_BYTE(start[i + 2], start[i + 3]));

            ret = fs_write(&file, (void *)&v, sizeof(v));
            if (ret != 2) {
                LOG_ERR("fs_write failed %d", ret);
            }
        }
    }

    ret = fs_close(&file);
    if (ret != 0) {
        LOG_ERR("fs_close failed: %d", ret);
    }

    return ROWS_TO_BUFFER;
}

void update_progress(size_t rows_buffered, float render_progress) {
    float p = ((float)rows_buffered) / ((float)IMAGE_HEIGHT) * 100.0;
    char msg[128];
    snprintf(msg, sizeof(msg), "buffering: %d %%\nrendering: %3.0f %%", (int)p, render_progress);
    lv_label_set_text(progress_label, msg);
    display_handler();
}

void fetch_image(void) {
    int ret;

    struct fs_file_t file;
    fs_file_t_init(&file);
    ret = fs_open(&file, TRANSFERRED_IMAGE_PATH, FS_O_CREATE | FS_O_WRITE);
    if (ret != 0) {
        LOG_ERR("fs_open failed: %d", ret);
    }
    ret = fs_truncate(&file, 0);
    if (ret != 0) {
        LOG_ERR("fs_truncate to 0 failed: %d", ret);
    }
    ret = fs_truncate(&file, sizeof(lv_img_header_t) + IMG_DATA_SIZE);
    if (ret != 0) {
        LOG_ERR("fs_truncate to IMG_DATA_SIZE failed: %d", ret);
    }
    ret = fs_seek(&file, 0, FS_SEEK_SET);
    if (ret != 0) {
        LOG_ERR("fs_seek to 0 failed: %d", ret);
    }

    lv_img_header_t header;
    header.cf = LV_IMG_CF_TRUE_COLOR;
    header.h = IMAGE_HEIGHT;
    header.w = IMAGE_WIDTH;
    ret = fs_write(&file, &header, sizeof(header));
    if (ret != sizeof(header)) {
        LOG_ERR("fs_write of lv_img_header_t header failed: %d", ret);
    }

    ret = fs_close(&file);
    if (ret != 0) {
        LOG_ERR("fs_close failed: %d", ret);
    }

    picture = show_picture(TRANSFERRED_IMAGE_PATH);

    size_t rows_buffered = 0;

    const uint8_t row_bundle_0[] = {0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224};
    const uint8_t row_bundle_1[] = {8, 24, 40, 56, 72, 88, 104, 120, 136, 152, 168, 184, 200, 216, 232};
    const uint8_t row_bundle_2[] = {4, 12, 20, 28, 36, 44, 52, 60, 68, 76, 84, 92, 100, 108, 116, 124, 132, 140, 148, 156, 164, 172, 180, 188, 196, 204, 212, 220, 228, 236};
    const uint8_t row_bundle_3[] = {2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 62, 66, 70, 74, 78, 82, 86, 90, 94, 98, 102, 106, 110, 114, 118, 122, 126, 130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190, 194, 198, 202, 206, 210, 214, 218, 222, 226, 230, 234, 238};
    const uint8_t *rows[] = {row_bundle_0, row_bundle_1, row_bundle_2, row_bundle_3};
    const size_t rows_lengths[] = {sizeof(row_bundle_0), sizeof(row_bundle_1), sizeof(row_bundle_2), sizeof(row_bundle_3)};

    for (size_t i = 0; i < sizeof(*rows); i++) {
        for (size_t j = 0; j < rows_lengths[i]; j++) {
            rows_buffered += render_row(rows[i][j]);
            float render_progress = (float)j / rows_lengths[i] * 100.0;
            update_progress(rows_buffered, render_progress);
        }

        k_msleep(50);
        lv_obj_invalidate(picture); // similar to lv_img_set_src(picture, TRANSFERRED_IMAGE_PATH); // calling to too often leads to memory fragmentation and eventually render errors doe to OOM issues in LVGL
        display_handler();
    }

    lv_label_set_text(progress_label, "Image complete!");
    display_handler();

    expresslink_send_command("AT+OTA CLOSE\n", NULL, 0);
    LOG_INF("AWS IoT Job completed!");
    LOG_INF("Image downloaded and rendered!");
}

static void init_ui_display() {
    set_display_brightness(100);
    lv_obj_clean(lv_scr_act());

    progress_label = lv_label_create(lv_scr_act());
    lv_obj_align(progress_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_align(progress_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(progress_label, "No pending OTA job.");
}

static void cleanup_ui_display() {
    delete_picture();
    picture = NULL;
    if (preload) {
        lv_obj_del(preload);
        preload = NULL;
    }
    if (progress_label) {
        lv_obj_del(progress_label);
        progress_label = NULL;
    }
    display_handler();
}

void image_transfer(void *context, void *dummy1, void *dummy2) {
    expresslink_response = k_malloc(expresslink_response_length);
    if (expresslink_response == NULL) {
        LOG_ERR("k_malloc for expresslink_response failed!");
        return;
    }

    init_ui_display();

    expresslink_reset();

    bool ota_in_progress = false;

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'Image Transfer' module.");

            expresslink_reset();
            k_free(expresslink_response);
            expresslink_response = NULL;

            cleanup_ui_display();
            return;
        }

        display_handler();

        if (!expresslink_check_event_pending()) {
            k_msleep(10);
            continue;
        }

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
                LOG_INF("Waiting for OTA jobs...");
            } else {
                LOG_INF("Connection attempt failed! Reconnecting...");
                k_msleep(3000);
                expresslink_send_command("AT+CONNECT!\n", NULL, 0);
            }
        } else if (expresslink_is_event(expresslink_response, EL_EVENT_OTA)) {
            int parameter = atoi(expresslink_response + 2);
            if (parameter == 2 && !ota_in_progress) {
                LOG_INF("New Host OTA image proposed!");
                expresslink_send_command("AT+OTA ACCEPT\n", NULL, 0);

                ota_in_progress = true;

                lv_label_set_text(progress_label, "Downloading image...");

                // clean up any previous image
                delete_picture();
                picture = NULL;

                preload = lv_spinner_create(lv_scr_act(), 1000, 60);
                lv_obj_set_size(preload, 150, 150);
                lv_obj_center(preload);
            } else if (parameter == 5) {
                LOG_INF("Host OTA image arrived!");
                if (preload) {
                    lv_obj_del(preload);
                    preload = NULL;
                }
                fetch_image();
                ota_in_progress = false;
            }
        } else {
            LOG_INF("Ignoring unhandled ExpressLink event: %s", expresslink_response);
        }
    }
}
