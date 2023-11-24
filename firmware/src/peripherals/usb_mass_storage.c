// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <ff.h>

#include "badge.h"

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/usb/class/usbd_msc.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
LOG_MODULE_REGISTER(usb_mass_storage);

#define USB_STORAGE_PARTITION usb_partition
#define USB_STORAGE_PARTITION_ID FIXED_PARTITION_ID(USB_STORAGE_PARTITION)

#define USB_VOLUME_LABEL "DEMO_BADGE"

#define COMPANION_APP_FILEPATH USB_PATH("companion_app.txt")
#define ACTIVE_WORKSHOP_MODULE_FILEPATH USB_PATH("active_workshop_module.txt")
#define EXPRESSLINK_PROVISIONING_FILEPATH USB_PATH("aws_iot_expresslink_provisioning.txt")
#define SIDEWALK_WIPE_FILEPATH USB_PATH("amazon_sidewalk_wipe.txt")

#if FF_USE_LABEL == 0
// This file requires the `f_setlabel` function from FATFS.

// This function needs to be enabled by:
//  modules/fs/fatfs/include/ffconf.h
//    #define FF_USE_LABEL 1

// It is not yet available in Zephyr-based FATFS config: zephyr/modules/fatfs/zephyr_fatfs_config.h

#error "Required setting missing in modules/fs/fatfs/include/ffconf.h: set FF_USE_LABEL to 1."
#endif

static FATFS fat_fs;
static struct fs_mount_t fs_mnt = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
    // .flags = FS_MOUNT_FLAG_READ_ONLY,
    .mnt_point = "/" CONFIG_MASS_STORAGE_DISK_NAME ":" // needs to match the disk-name of 'zephyr,flash-disk' device tree item
};

void restore_active_workshop_module() {
    int ret;
    struct fs_dirent dirent;

    struct fs_file_t file;
    fs_file_t_init(&file);

    ret = fs_stat(ACTIVE_WORKSHOP_MODULE_FILEPATH, &dirent);
    if (ret != 0 || dirent.size == 0) {
        run_without_storing(WORKSHOP_MODULE_WELCOME_SCREEN, welcome_screen);
        return;
    }

    ret = fs_open(&file, ACTIVE_WORKSHOP_MODULE_FILEPATH, FS_O_READ);
    if (ret != 0) {
        run_without_storing(WORKSHOP_MODULE_WELCOME_SCREEN, welcome_screen);
        return;
    }

    char name[32] = {0};
    ret = fs_read(&file, name, sizeof(name));
    if (ret < 1) {
        run_without_storing(WORKSHOP_MODULE_WELCOME_SCREEN, welcome_screen);
        return;
    }
    fs_close(&file);

    if (strcmp(name, WORKSHOP_MODULE_MQTT_PUB_SUB) == 0) {
        run_without_storing(name, mqtt_pub_sub);
    } else if (strcmp(name, WORKSHOP_MODULE_DEVICE_LOCATION) == 0) {
        run_without_storing(name, device_location);
    } else if (strcmp(name, WORKSHOP_MODULE_SENSOR_DATA_INGESTION) == 0) {
        run_without_storing(name, sensor_data_ingestion);
    } else if (strcmp(name, WORKSHOP_MODULE_DIGITAL_TWIN_AND_SHADOW) == 0) {
        run_without_storing(name, digital_twin_and_shadow);
    } else if (strcmp(name, WORKSHOP_MODULE_IMAGE_TRANSFER) == 0) {
        run_without_storing(name, image_transfer);
    } else if (strcmp(name, WORKSHOP_MODULE_SIDEWALK) == 0) {
        run_without_storing(name, start_sidewalk_sample);
    } else if (strcmp(name, WORKSHOP_MODULE_BLE_SENSOR_PERIPHERAL) == 0) {
        run_without_storing(name, ble_sensor_peripheral);
    } else {
        LOG_WRN("Unknown workshop module name: %s - running welcome_screen instead!", name);
        run_without_storing(WORKSHOP_MODULE_WELCOME_SCREEN, welcome_screen);
    }
}

void store_active_workshop_module(const char *name) {
    int ret;
    struct fs_file_t file;
    fs_file_t_init(&file);

    ret = fs_open(&file, ACTIVE_WORKSHOP_MODULE_FILEPATH, FS_O_CREATE | FS_O_WRITE);
    if (ret != 0) {
        LOG_WRN("store_active_workshop_module fs_open failed: %d", ret);
        return;
    }
    ret = fs_truncate(&file, 0);
    if (ret != 0) {
        LOG_WRN("store_active_workshop_module fs_truncate failed: %d", ret);
        return;
    }
    ret = fs_write(&file, name, strlen(name));
    if (ret < 0) {
        LOG_WRN("store_active_workshop_module fs_write failed: %d", ret);
        return;
    }

    LOG_DBG("workshop module: persisting %s|%d", name, strlen(name));
    fs_close(&file);
}

static int set_fatfs_volume_label() {
    char label[64];
    uint32_t vsn;
    int ret = f_getlabel(CONFIG_MASS_STORAGE_DISK_NAME ":", label, &vsn);
    if (ret == FR_OK && strcmp(label, USB_VOLUME_LABEL) != 0) {
        ret = f_setlabel(CONFIG_MASS_STORAGE_DISK_NAME ":" USB_VOLUME_LABEL);
        if (ret != FR_OK) {
            LOG_WRN("FATFS failed to set label: %d", ret);
            return -1;
        }
        LOG_DBG("FATFS volume label set to %s.", USB_VOLUME_LABEL);
        k_msleep(250);

        // reboot to allow Host to discover new volume label and auto-mount
        sys_reboot(SYS_REBOOT_WARM);
    }
    return ret;
}

void expresslink_provisioning() {
    int ret;
    struct fs_dirent dirent;

    struct fs_file_t file;
    fs_file_t_init(&file);

    ret = fs_stat(EXPRESSLINK_PROVISIONING_FILEPATH, &dirent);
    if (ret != 0 || dirent.size == 0) {
        return;
    }

    ret = fs_open(&file, EXPRESSLINK_PROVISIONING_FILEPATH, FS_O_READ);
    if (ret != 0) {
        LOG_ERR("failed to read %s", EXPRESSLINK_PROVISIONING_FILEPATH);
        return;
    }

    LOG_INF("ExpressLink provisioning...");

    char cmd[256] = {0};
    size_t pos = 0;
    while (true) {
        ret = fs_read(&file, cmd + pos, 1);
        if (ret == 1 && cmd[pos] == '\n') {
            cmd[pos+1] = 0x0;
            pos = 0;
            expresslink_send_command(cmd, NULL, 0);
            if (strcmp(cmd, "AT+FACTORY_RESET\n") == 0) {
                k_msleep(2500);
            } else {
                k_msleep(250);
            }
            continue;
        } else if (ret == 1) {
            pos++;
            if (ret > sizeof(cmd) - 1) {
                LOG_ERR("ExpressLink provisioning: command to long: %.*s", sizeof(cmd), cmd);
                fs_close(&file);
                return;
            }
        } else if (ret == 0) {
            break;
        }
    }

    fs_close(&file);
    fs_unlink(EXPRESSLINK_PROVISIONING_FILEPATH);

    LOG_INF("ExpressLink provisioning completed! Rebooting now...");
    k_msleep(250);
    NVIC_SystemReset(); // necessary to clear out the USB cache and FAT table cache in the Host OS
}

void sidewalk_wiping() {
    int ret;
    struct fs_dirent dirent;

    struct fs_file_t file;
    fs_file_t_init(&file);

    ret = fs_stat(SIDEWALK_WIPE_FILEPATH, &dirent);
    if (ret != 0) {
        return;
    }

    sidewalk_wipe();

    fs_unlink(SIDEWALK_WIPE_FILEPATH);

    LOG_INF("Sidewalk wiping completed! Rebooting now...");
    k_msleep(250);
    NVIC_SystemReset(); // necessary to clear out the USB cache and FAT table cache in the Host OS
}

void usb_mass_storage_wiping(const struct shell *sh, size_t argc, char **argv) {
    const char safety_check_arg[] = "--yes-i-am-sure";
    if (argc != 2 && strcmp(argv[1], safety_check_arg) != 0) {
        LOG_WRN("Wiping request ignored. Missing argument: %s", safety_check_arg);
        return;
    }

    LOG_WRN("Wiping all data... this might take a minute...");

    wipe_partition(USB_STORAGE_PARTITION_ID, FIXED_PARTITION_SIZE(USB_STORAGE_PARTITION));

    LOG_INF("Wiping completed! Rebooting now...");
    k_msleep(250);
    NVIC_SystemReset(); // necessary to automatically re-format the external flash with a working FATFS
}

int init_usb_mass_storage() {
    int ret;

    ret = fs_mount(&fs_mnt);
    if (ret == 0) {
        LOG_DBG("FATFS mounted.");
    } else {
        // might fail with NCS v2.4.0 or older!
        LOG_ERR("FATFS mounting or formatting failed!");
        return -1;
    }

    ret = set_fatfs_volume_label();
    if (ret != 0) {
        return ret;
    }

    const struct shell *sh = shell_backend_uart_get_ptr();
    expresslink_export_certificate(sh, 0, NULL, false);

    ret = expresslink_over_the_wire_update("v2.5.0.bin", "2.5.0", false);
    if (ret != 0) {
        led_strip_set_brightness(50);
        while (true) {
            led_strip_set_pixel(0, 255, 0, 0);
            led_strip_set_pixel(1, 255, 0, 0);
            led_strip_set_pixel(2, 255, 0, 0);
            k_msleep(1000);
            led_strip_set_pixel(0, 0, 0, 0);
            led_strip_set_pixel(1, 0, 0, 0);
            led_strip_set_pixel(2, 0, 0, 0);
            k_msleep(1000);
        }
    }

    expresslink_provisioning();

    sidewalk_wiping();

    return 0;
}
