// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "badge.h"

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/byteorder.h>
LOG_MODULE_REGISTER(sidewalk_persistence);

#define SIDEWALK_MFG_PARTITION mfg_storage
#define SIDEWALK_MFG_PARTITION_ID FIXED_PARTITION_ID(SIDEWALK_MFG_PARTITION)
#define SIDEWALK_MFG_SIZE FIXED_PARTITION_SIZE(SIDEWALK_MFG_PARTITION)

#define SIDEWALK_MFG_FILEPATH USB_PATH("amazon_sidewalk_mfg.bin")
#define SIDEWALK_MFG_HEADER "SID0"

int sidewalk_wipe() {
    int ret = wipe_partition(SIDEWALK_MFG_PARTITION_ID, SIDEWALK_MFG_SIZE);
    ret += wipe_partition(FIXED_PARTITION_ID(sidewalk_storage), FIXED_PARTITION_SIZE(sidewalk_storage));
    ret += wipe_partition(FIXED_PARTITION_ID(storage_partition), FIXED_PARTITION_SIZE(storage_partition));
    LOG_INF("Sidewalk provisioning data wiped.");
    return ret;
}

int init_sidewalk_mfg_payload() {
    int ret;

    // check file metadata
    struct fs_dirent dirent;
    ret = fs_stat(SIDEWALK_MFG_FILEPATH, &dirent);
    if (ret == -ENOENT) {
        // file not found, which is fine
        return 0;
    } else if (ret != 0) {
        LOG_ERR("Provisioning: failed to stat %s: %d", SIDEWALK_MFG_FILEPATH, ret);
        return ret;
    } else if (dirent.size == 0 || dirent.size < 1024 || dirent.size > SIDEWALK_MFG_SIZE) {
        LOG_ERR("Provisioning file: unexpected size: %d bytes", dirent.size);
        return -1;
    }

    // open file
    struct fs_file_t file;
    fs_file_t_init(&file);
    ret = fs_open(&file, SIDEWALK_MFG_FILEPATH, FS_O_READ);
    if (ret != 0) {
        LOG_ERR("Provisioning: failed to open %s: %d", SIDEWALK_MFG_FILEPATH, ret);
        return ret;
    }

    uint8_t *file_buf = k_malloc(SIDEWALK_MFG_SIZE);

    // read magic header from file
    size_t count = 0;
    while (count < sizeof(SIDEWALK_MFG_HEADER)) {
        size_t read = fs_read(&file, file_buf + count, sizeof(SIDEWALK_MFG_HEADER) - count);
        if (read == 0) {
            // end of file reached
            break;
        }
        count += read;
    }

    // check magic header
    if (count < sizeof(SIDEWALK_MFG_HEADER) || memcmp(file_buf, SIDEWALK_MFG_HEADER, sizeof(SIDEWALK_MFG_HEADER)) != 0) {
        LOG_ERR("Provisioning: unexpected file header");
        k_free(file_buf);
        return -1;
    }

    const struct flash_area *mfg_storage;
    ret = flash_area_open(SIDEWALK_MFG_PARTITION_ID, &mfg_storage);
    if (ret != 0) {
        LOG_ERR("Provisioning: failed to open flash area: %d", ret);
        k_free(file_buf);
        return ret;
    }

    uint8_t *internal_flash_buf = k_malloc(SIDEWALK_MFG_SIZE);

    // read the full file into a buffer
    fs_seek(&file, 0, FS_SEEK_SET);
    count = 0;
    while (count < dirent.size) {
        size_t read = fs_read(&file, file_buf + count, dirent.size - count);
        if (read == 0) {
            // end of file reached
            break;
        }
        count += read;
    }
    // actual file size must be <= flash area size
    if (fs_tell(&file) != dirent.size || count != dirent.size) {
        ret = -1;
        goto cleanup;
    }

    // read the full flash partition into a buffer
    ret = flash_area_read(mfg_storage, 0, internal_flash_buf, SIDEWALK_MFG_SIZE);
    if (ret != 0) {
        LOG_ERR("Provisioning: failed to read from internal flash: %d", ret);
        goto cleanup;
    }

    // compare the file against the content of the internal flash area
    if (memcmp(file_buf, internal_flash_buf, count) == 0) {
        // already imported and content matches, no further action needed
        ret = 0;
        goto cleanup;
    }

    LOG_INF("Provisioning: importing from file...");

    // wipe everything
    ret = sidewalk_wipe();
    if (ret != 0) {
        goto cleanup;
    }

    // write the full file to the internal flash area
    ret = flash_area_write(mfg_storage, 0, file_buf, SIDEWALK_MFG_SIZE);
    if (ret != 0) {
        LOG_ERR("Provisioning: failed to write to flash: %d", ret);
        goto cleanup;
    }

    LOG_INF("Provisioning: successfully imported.");
    ret = 0;

cleanup:
    fs_close(&file);
    flash_area_close(mfg_storage);
    k_free(file_buf);
    k_free(internal_flash_buf);
    return ret;
}

int inject_sidewalk_mfg_payload(const struct shell *sh, size_t argc, char **argv) {
    int ret;
    struct fs_file_t file;
    fs_file_t_init(&file);
    char *payload = NULL;

    if (argc == 3 && strcmp(argv[1], "-s") == 0) {
        // start a new file by deleting the old one first
        struct fs_dirent dirent;
        ret = fs_stat(SIDEWALK_MFG_FILEPATH, &dirent);
        if (ret == 0) {
            fs_unlink(SIDEWALK_MFG_FILEPATH); // ignore potential error
        }

        payload = argv[2];

        if (
            strlen(payload) < sizeof(SIDEWALK_MFG_HEADER) * 2 ||
            (char)HEX_TO_BYTE(payload[0], payload[1]) != SIDEWALK_MFG_HEADER[0] ||
            (char)HEX_TO_BYTE(payload[2], payload[3]) != SIDEWALK_MFG_HEADER[1] ||
            (char)HEX_TO_BYTE(payload[4], payload[5]) != SIDEWALK_MFG_HEADER[2] ||
            (char)HEX_TO_BYTE(payload[6], payload[7]) != SIDEWALK_MFG_HEADER[3]) {
            LOG_ERR("invalid file header, expected %s", SIDEWALK_MFG_HEADER);
            return -1;
        }
    } else if (argc == 3 && strcmp(argv[1], "-a") == 0) {
        // continue to append
        payload = argv[2];
    } else if (argc == 2 && strcmp(argv[1], "-e") == 0) {
        // empty end call
        return init_sidewalk_mfg_payload();
    } else if (argc == 2 && strcmp(argv[1], "-w") == 0) {
        return sidewalk_wipe();
    } else {
        LOG_ERR("expected first command to be with '-s' argument to start a new file, or '-e' to end the file!");
        return -1;
    }

    ret = fs_open(&file, SIDEWALK_MFG_FILEPATH, FS_O_CREATE | FS_O_WRITE);
    if (ret != 0) {
        return ret;
    }

    ret = fs_seek(&file, 0, FS_SEEK_END);
    if (ret != 0) {
        return ret;
    }

    size_t written = 0;

    size_t payload_len = strlen(payload);
    for (size_t i = 0; i < payload_len; i += 2) {
        uint8_t b = HEX_TO_BYTE(payload[i], payload[i + 1]);
        written += fs_write(&file, &b, 1);
    }

    fs_close(&file);

    shell_print(sh, "Written %u bytes to %s.", written, SIDEWALK_MFG_FILEPATH);

    return 0;
}
