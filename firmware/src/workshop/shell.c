// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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
#include <zephyr/kernel_version.h>
#include "version.h"
LOG_MODULE_REGISTER(workshop_shell);

#include <ncs_version.h>

#include "app_version.h"
#include "sidewalk_version.h"
#include "badge.h"

static int cmd_enter_bootloader(const struct shell *sh, size_t argc, char **argv) {
    // see https://github.com/adafruit/Adafruit_nRF52_Bootloader/tree/7210c3914db0cf28e7b2c9850293817338259757#how-to-use
    NRF_POWER->GPREGRET = 0x57; // 0xA8 OTA, 0x4e Serial
    NVIC_SystemReset();
    return 0;
}

static int cmd_info(const struct shell *sh, size_t argc, char **argv) {
    shell_print(sh, "--------------------");
    shell_print(sh, "This is the AWS IoT - Connected Product - Demo Badge, 2023 edition.");
    shell_print(sh, "It was designed and built by:");
    shell_print(sh, "  Thomas Kriechbaumer (https://linkedin.com/in/thomas-kriechbaumer)");
    shell_print(sh, "");
    shell_print(sh, "This device and related workshop content serve as educational tool kit to explore AWS IoT services and features in the context of connected products.");
    shell_print(sh, "");
    shell_print(sh, "All resources are open-source and available at:");
    shell_print(sh, "  https://github.com/aws-samples/aws-iot-connected-product-demo-badge");
    shell_print(sh, "");
    shell_print(sh, "Demo Badge firmware version: %s", APP_GIT_TS);
    shell_print(sh, "");
    shell_print(sh, "Zephyr version: v%s", KERNEL_VERSION_STRING);
    shell_print(sh, "nRF Connect NCS version: v%s", NCS_VERSION_STRING);
    shell_print(sh, "Sidewalk:");
    shell_print(sh, "  build time: %s", build_time_stamp);

    for (int i = 0; i < sidewalk_version_component_count; i++) {
		shell_print(sh, "  %s: %s", sidewalk_version_component_name[i], sidewalk_version_component[i]);
	}

    shell_print(sh, "--------------------");
    return 0;
}

static int cmd_self_test(const struct shell *sh, size_t argc, char **argv) {
    run(WORKSHOP_MODULE_SELF_TEST, self_test);
    return 0;
}

static int cmd_mqtt_pub_sub(const struct shell *sh, size_t argc, char **argv) {
    run(WORKSHOP_MODULE_MQTT_PUB_SUB, mqtt_pub_sub);
    return 0;
}

static int cmd_device_location(const struct shell *sh, size_t argc, char **argv) {
    run(WORKSHOP_MODULE_DEVICE_LOCATION, device_location);
    return 0;
}

static int cmd_sensor_data_ingestion(const struct shell *sh, size_t argc, char **argv) {
    int32_t update_rate = 0;
    if (argc == 2) {
        update_rate = atoi(argv[1]);
    }
    run_args(WORKSHOP_MODULE_SENSOR_DATA_INGESTION, sensor_data_ingestion, true, (void *)update_rate, NULL, NULL);
    return 0;
}

static int cmd_digital_twin_and_shadow(const struct shell *sh, size_t argc, char **argv) {
    run(WORKSHOP_MODULE_DIGITAL_TWIN_AND_SHADOW, digital_twin_and_shadow);
    return 0;
}

static int cmd_image_transfer(const struct shell *sh, size_t argc, char **argv) {
    run(WORKSHOP_MODULE_IMAGE_TRANSFER, image_transfer);
    return 0;
}

static int cmd_sidewalk(const struct shell *sh, size_t argc, char **argv) {
    run(WORKSHOP_MODULE_SIDEWALK, start_sidewalk_sample);
    return 0;
}

static int cmd_ble_sensor_peripheral(const struct shell *sh, size_t argc, char **argv) {
    run(WORKSHOP_MODULE_BLE_SENSOR_PERIPHERAL, ble_sensor_peripheral);
    return 0;
}

static int cmd_sidewalk_provisioning(const struct shell *sh, size_t argc, char **argv) {
    return inject_sidewalk_mfg_payload(sh, argc, argv);
}

static int cmd_wipe_usb_mass_storage(const struct shell *sh, size_t argc, char **argv) {
    usb_mass_storage_wiping(sh, argc, argv);
    return 0;
}

static int cmd_stop(const struct shell *sh, size_t argc, char **argv) {
    store_active_workshop_module(""); // nuke it
    run_without_storing(WORKSHOP_MODULE_WELCOME_SCREEN, welcome_screen);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_run,
	SHELL_CMD_ARG(mqtt_pub_sub, NULL, "Run the 'MQTT Publish/Subscribe' workshop module", cmd_mqtt_pub_sub, 1, 0),
	SHELL_CMD_ARG(device_location, NULL, "Run the 'Device Location' workshop module", cmd_device_location, 1, 0),
	SHELL_CMD_ARG(sensor_data_ingestion, NULL, "Run the 'Sensor Data Ingestion' workshop module", cmd_sensor_data_ingestion, 1, 1),
	SHELL_CMD_ARG(digital_twin_and_shadow, NULL, "Run the 'Digital Twin and Shadow' workshop module", cmd_digital_twin_and_shadow, 1, 0),
	SHELL_CMD_ARG(image_transfer, NULL, "Run the 'Image Transfer' workshop module", cmd_image_transfer, 1, 0),
	SHELL_CMD_ARG(sidewalk, NULL, "Run the 'Sidewalk' workshop module", cmd_sidewalk, 1, 0),
	SHELL_CMD_ARG(ble_sensor_peripheral, NULL, "Run the 'BLE Sensor Peripheral' workshop module", cmd_ble_sensor_peripheral, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_workshop,
	SHELL_CMD(run, &sub_run, "Run a workshop module", NULL),
	SHELL_CMD(stop, NULL, "Stop any running workshop module", cmd_stop),
	SHELL_CMD(self_test, NULL, "Run Demo Badge self test", cmd_self_test),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(enter_bootloader, NULL, "Enter bootloader for UF2 flashing", cmd_enter_bootloader);

SHELL_CMD_REGISTER(info, NULL, "Show detailed information about this device", cmd_info);

SHELL_CMD_REGISTER(workshop, &sub_workshop, "Workshop commands", NULL);

SHELL_CMD_REGISTER(sidewalk_provisioning, NULL, "Append to Sidewalk MFG payload (use '-s' '-a' '-e' to start/append/end the file)", cmd_sidewalk_provisioning);

SHELL_CMD_REGISTER(wipe_usb_mass_storage, NULL, "Wipe flash region for USB mass storage. WARNING: all data files are unrecoverably deleted!", cmd_wipe_usb_mass_storage);
