// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef BADGE_H
#define BADGE_H

#include <lvgl.h>
#include <stdbool.h>
#include <stdio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/shell/shell.h>

#include "peripherals/expresslink.h"

#define USB_PATH(file) ( "/" CONFIG_MASS_STORAGE_DISK_NAME ":/" file )

#define HEX_TO_BYTE(_a, _b) (((((_a) % 32 + 9) % 25) << 4) + (((_b) % 32 + 9) % 25))

#define WORKSHOP_MODULE_WELCOME_SCREEN "welcome_screen"
#define WORKSHOP_MODULE_SELF_TEST "self_test"
#define WORKSHOP_MODULE_MQTT_PUB_SUB "mqtt_pub_sub"
#define WORKSHOP_MODULE_DEVICE_LOCATION "device_location"
#define WORKSHOP_MODULE_SENSOR_DATA_INGESTION "sensor_data_ingestion"
#define WORKSHOP_MODULE_DIGITAL_TWIN_AND_SHADOW "digital_twin_and_shadow"
#define WORKSHOP_MODULE_IMAGE_TRANSFER "image_transfer"
#define WORKSHOP_MODULE_SIDEWALK "sidewalk"
#define WORKSHOP_MODULE_BLE_SENSOR_PERIPHERAL "ble_sensor_peripheral"

int init_primary_thread(void);
int init_user_led(void);
int init_buttons(void);
int init_ambient_light(void);
int init_sht31(void);
int init_lsm6dsl(void);
int init_led_strip(void);
int init_expresslink(void);
int init_display(void);
int init_usb_mass_storage(void);
int init_settings(void);
int init_sidewalk_mfg_payload(void);

void run(const char *name, void *function);
void run_without_storing(const char *name, void *function);
void run_args(const char *name, void *function, bool store, void *p1, void *p2, void *p3);
bool shutdown_request_received(void);
void gracefully_shutdown_primary_thread(void);

void store_active_workshop_module(const char *);
void restore_active_workshop_module(void);

int sidewalk_wipe(void);
int inject_sidewalk_mfg_payload(const struct shell *sh, size_t argc, char **argv);
void usb_mass_storage_wiping(const struct shell *sh, size_t argc, char **argv);

void self_test(void *, void *, void *);
void welcome_screen(void *, void *, void *);
void mqtt_pub_sub(void *, void *, void *);
void device_location(void *, void *, void *);
void sensor_data_ingestion(void *, void *, void *);
void digital_twin_and_shadow(void *, void *, void *);
void image_transfer(void *, void *, void *);
void start_sidewalk_sample(void *, void *, void *);
void ble_sensor_peripheral(void *, void *, void *);

bool shell_check_arg_truthy(char *arg);
bool shell_check_arg_falsy(char *arg);

int wipe_partition(uint8_t partition_id, size_t partition_size);

void turn_user_led_on(void);
void turn_user_led_off(void);
void toggle_user_led(void);

void led_strip_refresh(void);
void led_strip_set_brightness(uint8_t value);
void led_strip_set_pixel(size_t index, uint8_t r, uint8_t g, uint8_t b);

void display_handler();
void display_handler_async();
lv_obj_t *show_picture(const char *path);
void delete_picture();
lv_obj_t *show_qr_code(const char *url);
void delete_qr_code();
void set_display_brightness(int v);

typedef struct init_retcode_t {
    int8_t primary_thread;
    int8_t user_led;
    int8_t led_strip;
    int8_t buttons;
    int8_t ambient_light;
    int8_t sht31;
    int8_t lsm6dsl;
    int8_t expresslink;
    int8_t display;
    int8_t usb_mass_storage;
    int8_t settings;
} init_retcode_t;

typedef struct sht3xd_sample {
    float32_t temperature;
    float32_t humidity;
} sht3xd_sample;

int read_sht31_sample(sht3xd_sample *value);

typedef struct lsm6dsl_sample {
    float32_t accel_x;
    float32_t accel_y;
    float32_t accel_z;
    float32_t angular_velocity_x;
    float32_t angular_velocity_y;
    float32_t angular_velocity_z;
} lsm6dsl_sample;

int read_lsm6dsl_sample(lsm6dsl_sample *value);

int16_t read_ambient_light(void);

volatile extern bool button1_pressed;
volatile extern bool button2_pressed;
volatile extern bool button3_pressed;
volatile extern bool button4_pressed;

bool workshop_wifi_device_location_override(char *response, size_t response_len);

#endif // BADGE_H
