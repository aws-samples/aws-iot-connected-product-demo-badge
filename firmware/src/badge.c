// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(demo_badge);

bool shell_check_arg_truthy(char *arg) {
    return (
        strcmp(arg, "1") == 0 ||
        strcmp(arg, "true") == 0 ||
        strcmp(arg, "True") == 0 ||
        strcmp(arg, "y") == 0 ||
        strcmp(arg, "Y") == 0 ||
        strcmp(arg, "yes") == 0 ||
        strcmp(arg, "Yes") == 0 ||
        strcmp(arg, "on") == 0 ||
        strcmp(arg, "On") == 0 ||
        strcmp(arg, "ON") == 0);
}

bool shell_check_arg_falsy(char *arg) {
    return (
        strcmp(arg, "0") == 0 ||
        strcmp(arg, "false") == 0 ||
        strcmp(arg, "False") == 0 ||
        strcmp(arg, "n") == 0 ||
        strcmp(arg, "N") == 0 ||
        strcmp(arg, "no") == 0 ||
        strcmp(arg, "No") == 0 ||
        strcmp(arg, "off") == 0 ||
        strcmp(arg, "Off") == 0 ||
        strcmp(arg, "OFF") == 0);
}

int wipe_partition(uint8_t partition_id, size_t partition_size) {
    const struct flash_area *partition;
    int ret = flash_area_open(partition_id, &partition);
    if (ret != 0) {
        LOG_ERR("failed to open flash area with id %d: %d", partition_id, ret);
        return ret;
    }
    ret = flash_area_erase(partition, 0, partition_size);
    if (ret != 0) {
        LOG_ERR("failed to erase flash area with id %d and size %d: %d", partition_id, partition_size, ret);
        return ret;
    }
    flash_area_close(partition);
    return 0;
}
