// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "badge.h"

#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
LOG_MODULE_REGISTER(workshop_wifi_device_location_override);

#define WORKSHOP_WIFI_DEVICE_LOCATION_OVERRIDE_FILEPATH USB_PATH("workshop_wifi_device_location_override.txt")

bool workshop_wifi_device_location_override(char *response, const size_t response_len) {
    int ret;
    size_t buf_length = 1024;

    struct fs_dirent dirent;
    ret = fs_stat(WORKSHOP_WIFI_DEVICE_LOCATION_OVERRIDE_FILEPATH, &dirent);
    if (ret == -ENOENT || ret != 0 || dirent.size == 0) {
        // file not found, or stat err, or empty
        return false;
    }
    if (dirent.size > buf_length) {
        LOG_WRN("payload in file too long!");
        return false;
    }

    struct fs_file_t file;
    fs_file_t_init(&file);
    ret = fs_open(&file, WORKSHOP_WIFI_DEVICE_LOCATION_OVERRIDE_FILEPATH, FS_O_READ);
    if (ret != 0) {
        return false;
    }

    char *buf = k_malloc(buf_length);
    if (buf == NULL) {
        fs_close(&file);
        return false;
    }
    memset(buf, 0x0, buf_length);

    size_t read_len = fs_read(&file, buf, buf_length - 1);
    fs_close(&file);

    if (read_len == 1 && buf[0] == '\n') {
        k_free(buf);
        return false;
    }
    if (read_len > 3 && buf[read_len - 1] == '\n') {
        buf[read_len - 1] = 0x0;
        read_len -= 1;
    }
    if (read_len > 3 && buf[read_len - 1] == '\r') {
        buf[read_len - 1] = 0x0;
        read_len -= 1;
    }

    k_msleep(2000); // delay a bit to mimic the real behaviour of scanning for WiFi networks

    snprintf(response, response_len, "OK %.*s", read_len, buf);
    k_free(buf);
    return true;
}
