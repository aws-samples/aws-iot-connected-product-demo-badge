// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <assert.h>
#include <lvgl.h>
#include <stdio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(self_test);

#include "badge.h"
#include "self_test.h"

#include "app_version.h"

// #define SIMULATE_LED_STRIP_INIT_FAILURE
// #define SIMULATE_EXPRESSLINK_FAILURE
// #define SIMULATE_SHT3XD_FAILURE
// #define SIMULATE_FLASH_FAILURE
// #define SIMULATE_EL_EVENT_FAILURE

/*  externs */
extern const struct device *display_dev;
volatile extern init_retcode_t init_retcode;

// layout
#define ROWS 15
#define ROW_HEIGHT 14
#define ROW_X_OFFSET 5
#define ROW_Y_OFFSET 50

// rows
#define LSM6DSL_ROW 0
#define AMBIENT_ROW 1
#define SHT31_ROW 2
#define DISPLAY_ROW 3
#define EXPRESSLINK_ROW 4
#define LED_STRIP_ROW 5
#define USER_LED_ROW 6
#define USB_MASS_STORAGE_ROW 7
#define FLASH_ROW 8
#define BUTTONS_ROW 9
#define EL_EVENT_ROW 10

/*  global variables */
lv_obj_t *label_row[ROWS], *label_git_sha, *label_ts;
lv_style_t style_bg_popup_message;
lv_obj_t *ta_popup_message;

// labels
#define LSM6DSL_TEXT "LSM6DSL"
#define AMBIENT_LIGHT_TEXT "AMBIENT LIGHT"
#define SHT31_TEXT "SHT31"
#define DISPLAY_TEXT "DISPLAY"
#define EXPRESSLINK_TEXT "EXPRESSLINK"
#define BUTTONS_TEXT "Buttons"
#define WS2812_LEDS_TEXT "WS2812 LEDs"
#define USER_LED_TEXT "User LED"
#define EXTERNAL_FLASH_TEXT "External Flash"
#define EL_EVENT_TEXT "EL_EVENT"
#define USB_MASS_STORAGE_TEXT "USB mass-storage"

// tests criteria
#define TEST_AMBIENT_LIGHT_THRESHOLD 10
#define TEST_ACC_THRESHOLD 11
#define TEST_TEMP_MIN 10
#define TEST_TEMP_MAX 40
#define TEST_FILE_PATH USB_PATH("test-file.txt")

static void init_ui_display() {
    lv_obj_clean(lv_scr_act());

    // FW git sha and build timestamp info
    label_git_sha = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_align(label_git_sha, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label_git_sha, LV_ALIGN_TOP_MID, ROW_X_OFFSET, 10 + ROW_HEIGHT * 0);
    lv_label_set_text(label_git_sha, APP_GIT_SHA);

    label_ts = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_align(label_ts, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label_ts, LV_ALIGN_TOP_MID, ROW_X_OFFSET, 10 + ROW_HEIGHT * 1);
    lv_label_set_text(label_ts, APP_GIT_TS);

    // create label layout (1 label per each row)
    for (int i = 0; i < ROWS; i++) {
        label_row[i] = lv_label_create(lv_scr_act());

        lv_obj_align(label_row[i], LV_ALIGN_TOP_LEFT, ROW_X_OFFSET, ROW_Y_OFFSET + ROW_HEIGHT * i);
        lv_label_set_recolor(label_row[i], true);
        lv_label_set_text(label_row[i], "");
    }

    // passed / failed pop-up
    lv_style_init(&style_bg_popup_message);
    lv_style_set_bg_opa(&style_bg_popup_message, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bg_popup_message, lv_color_hex(0xff0000));
    lv_style_set_text_font(&style_bg_popup_message, &lv_font_montserrat_16);

    display_handler();
}

static void cleanup_ui_display() {
    for (size_t i = 0; i < ROWS; i++) {
        if (label_row[i]) {
            lv_obj_del(label_row[i]);
            label_row[i] = NULL;
        }
    }
    if (label_git_sha) {
        lv_obj_del(label_git_sha);
        label_git_sha = NULL;
    }
    if (label_ts) {
        lv_obj_del(label_ts);
        label_ts = NULL;
    }
    if (ta_popup_message) {
        lv_obj_del(ta_popup_message);
        ta_popup_message = NULL;
    }
    display_handler();
}

static bool wait_for_button(volatile bool *button) {
    *button = false;
    while (!(*button)) {
        if (shutdown_request_received()) {
            cleanup_ui_display();
            return false;
        }
        k_msleep(10);
    }
    return true;
}

/*
    Prints the 'label_text' on the given 'row'.
    If 'successful' is false (i.e. the test was not successful), it also appends 'text' and set the color to RED.
    It returns 'successful'
*/
static bool check_bool_and_print(bool successful, int row, char *label_text, char *text) {
    if (successful) {
        lv_label_set_text_fmt(label_row[row], LV_SYMBOL_OK " %2d - %s", row, label_text);
        LOG_INF("Testing %s: ok", label_text);
    } else {
        lv_label_set_text_fmt(label_row[row], LV_SYMBOL_CLOSE "#ff0000 %2d - %s (%s)#", row, label_text, text);
        LOG_ERR("Testing %s: ERROR (%s)", label_text, text);
    }

    display_handler();

    return successful;
}

/*
    Prints the 'label_text' on the given 'row'.
    If 'retcode' is not zero (i.e. the test was not successful), it also appends 'text' and set the color to RED.
    It returns:
    - true if 'retcode' == 0
    - false otherwise
*/
static bool check_int_and_print(int retcode, int row, char *label_text) {
    bool successful;

    char text[6];
    snprintf(text, sizeof(text), "%d", retcode);

    if (retcode == 0) {
        successful = true;
    } else {
        successful = false;
    }

    return check_bool_and_print(successful, row, label_text, text);
}

/*
    Tests the ambient light sensor by:
    - acquiring a single reading of the ambient light
    - checking whether it is > TEST_AMBIENT_LIGHT_THRESHOLD

    The result is then printed on screen on the AMBIENT_ROW row.

    It returns:
    - true if the test is successful
    - false otherwise
*/
bool test_ambient_light() {
    bool test_result;
    int16_t v = read_ambient_light();

    if (v > TEST_AMBIENT_LIGHT_THRESHOLD) {
        test_result = true;
    } else {
        test_result = false;
    }

    char text[8];
    snprintf(text, sizeof(text), "%05d", v);
    check_bool_and_print(test_result, AMBIENT_ROW, AMBIENT_LIGHT_TEXT, text);

    return test_result;
}

/*
    Tests the inertial sensor by:
    - acquiring a single reading
    - checking whether acceleration on each axis (x,y,z) is in the -TEST_ACC_THRESHOLD<.<TEST_ACC_THRESHOLD range

    The result is then printed on screen on the LSM6DSL_ROW row.

    It returns:
    - true if the test is successful
    - false otherwise
*/
bool test_lsm6dsl() {
    lsm6dsl_sample v;
    read_lsm6dsl_sample(&v);

    bool test_result = true;

    if (v.accel_x > TEST_ACC_THRESHOLD || v.accel_x < -TEST_ACC_THRESHOLD) {
        test_result &= false;
    }

    if (v.accel_y > TEST_ACC_THRESHOLD || v.accel_y < -TEST_ACC_THRESHOLD) {
        test_result &= false;
    }

    if (v.accel_z > TEST_ACC_THRESHOLD || v.accel_z < -TEST_ACC_THRESHOLD) {
        test_result &= false;
    }

    char text[32];
    snprintf(text, sizeof(text), "%02.2f/%02.2f/%02.2f", v.accel_x, v.accel_y, v.accel_z);
    check_bool_and_print(test_result, LSM6DSL_ROW, LSM6DSL_TEXT, text);

    return test_result;
}

/*
    Tests the temperature / humidity sensor by:
    - acquiring a single reading
    - checking whether the temperature is in the TEST_TEMP_MIN<.<TEST_TEMP_MAX range

    The result is then printed on screen on the SHT31_ROW row.

    It returns:
    - true if the test is successful
    - false otherwise

    NOTE: you can simulate a failure by defining SIMULATE_SHT3XD_FAILURE
*/
bool test_sht3xd() {
    bool test_result;

    sht3xd_sample v;
    read_sht31_sample(&v);

    // test: temp should be between 10 and 40

    if (v.temperature > TEST_TEMP_MIN && v.temperature < TEST_TEMP_MAX) {
        test_result = true;
    } else {
        test_result = false;
    }

#ifdef SIMULATE_SHT3XD_FAILURE
    test_result = false;
#endif

    char text[8];
    snprintf(text, sizeof(text), "%.1f", v.temperature);
    check_bool_and_print(test_result, SHT31_ROW, SHT31_TEXT, text);

    return test_result;
}

/*
    Tests the expresslink module UART i/f by:
    - sending the "AT" command
    - checking whether the response equals "OK"

    The result is then printed on screen on the EXPRESSLINK_ROW row.

    It returns:
    - 0 if the test is successful

    NOTE: you can simulate a failure by defining SIMULATE_EXPRESSLINK_FAILURE
*/
bool test_expresslink() {
    bool res;

    // send an "AT" command
    // (and the "expresslink_send_command" function will return true if response contains "OK" as expected)
    char response[48];
    res = expresslink_send_command("AT+CONF? Version\n", response, sizeof(response));

#ifdef SIMULATE_EXPRESSLINK_FAILURE
    res = false;
#endif

    const char *version = response;
    const char *old_version_string = "ExpressLink ";
    char *o = strstr(response, old_version_string);
    if (o != NULL) {
        version = response + strlen(old_version_string);
    }

    char text[64];
    snprintf(text, sizeof(text), "%s: %s", EXPRESSLINK_TEXT, version);
    check_bool_and_print(res, EXPRESSLINK_ROW, text, NULL);

    return res;
}

/*
    Resets the expresslink module by using the reset function (which includes an appropriate amount of sleep)
    NOTE: the response is not checked.

    It returns void.
*/
void test_expresslink_reset() {
    expresslink_reset();
}

/*
    Shows 'text' in a rectangle filled with 'color' to emulate a pop-up message.

    It returns void.
*/
void show_popup(char *text, int color) {
    if (ta_popup_message == NULL) {
        ta_popup_message = lv_textarea_create(lv_scr_act());
        lv_obj_set_size(ta_popup_message, 200, 40);
        lv_obj_set_style_text_align(ta_popup_message, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(ta_popup_message, LV_ALIGN_BOTTOM_MID, 0, -20);
    }

    lv_style_set_bg_color(&style_bg_popup_message, lv_color_hex(color));
    lv_obj_add_style(ta_popup_message, &style_bg_popup_message, 0);

    lv_textarea_set_text(ta_popup_message, text);

    display_handler();
}

/*
    Creates a file TEST_FILE_PATH and writes "hello" as content.
    The steps are:
    - fs_open
    - fs_write
    - fs_close

    It returns:
    - 0 if successful
    - !=0 in case of failure. It returns the error code of the step which failed.

    NOTE: you can simulate a failure by defining SIMULATE_FLASH_FAILURE.
*/
int test_create_file() {
    struct fs_file_t file;

#ifdef SIMULATE_FLASH_FAILURE
    const char payload[] = "heXXo";
#else
    const char payload[] = "hello";
#endif

    int rc, error;

    fs_file_t_init(&file);

    rc = fs_open(&file, TEST_FILE_PATH, FS_O_CREATE | FS_O_WRITE);
    if (rc != 0) {
        error = rc;
        return error;
    }

    rc = fs_write(&file, payload, sizeof(payload));
    if (rc != sizeof(payload)) {
        error = rc;
        return error;
    }

    rc = fs_close(&file);
    if (rc != 0) {
        error = rc;
        return error;
    }

    error = 0;
    return error;
}

/*
    Reads the file 'TEST_FILE_PATH' and checks whether teh content equals "hello".
    The steps are:
    - fs_open
    - fs_read
    - fs_close

    It returns:
    - 0 if successful
    - !=0 in case of failure. It returns the error code of the step which failed.
*/
int test_read_file() {
    struct fs_file_t file;
    const char payload[] = "hello";
    char buf[6];
    int rc, error;

    fs_file_t_init(&file);

    rc = fs_open(&file, TEST_FILE_PATH, FS_O_READ);
    if (rc != 0) {
        error = rc;
        return error;
    }

    rc = fs_read(&file, buf, sizeof(payload));
    if (rc != sizeof(payload)) {
        error = rc;
        return error;
    }

    rc = strcmp(payload, buf);
    if (rc != 0) {
        error = rc;
        return error;
    }

    rc = fs_close(&file);
    if (rc != 0) {
        error = rc;
        return error;
    }

    error = 0;
    return error;
}

/*
    Tests the flash by doing the following on the file-system the file 'TEST_FILE_PATH' and checks whether the content equals "hello".
    - creates a file TEST_FILE_PATH with content "hello"
    - reads the file and checks the content
    - deletes the file

    The result is then printed on screen on the FLASH_ROW row.

    It returns:
    - 0 if successful
    - !=0 in case of failure. It returns the error code of the step which failed.
*/
bool test_flash(void) {
    int rc;
    bool test_result = true;

    rc = test_create_file();
    if (rc != 0) {
        test_result &= false;
    }

    rc = test_read_file();
    if (rc != 0) {
        test_result &= false;
    }

    fs_unlink(TEST_FILE_PATH);

    char rc_str[8];
    snprintf(rc_str, sizeof(rc_str), "%d", rc);
    check_bool_and_print(test_result, FLASH_ROW, EXTERNAL_FLASH_TEXT, rc_str);

    return test_result;
}

/*
    This is the self-test task.

    Once called, it will:
    - init the labels on the display
    - check the global 'init_retcode' structure with the retcode of the init functions and shows the results
    - run the following self-tests and shows the results
        - test_flash
        - test_expresslink
        - test_ambient_light
        - test_lsm6dsl
        - test_sht3xd
    - ask the user to press buttons 1-2-3-4
        - on button 1, turns the strip LED 0 to RED
        - on button 2, turns the strip LED 1 to RED
        - on button 3, turns the strip LED 2 to RED
        - on button 4, turns ALL the strip LEDs to GREEN
        - turns the LED STRIP LED 1/2/3 to GREEN on buttons 2/3/4
    - reset the expresslink module to turn the EVENT LED on
    - check whether the EL_EVENT signal is high
    - turns the USER LED on

    The result of each step is shown on the display.
    If all the steps are successful, it will show a green "PASSED" pop-up message.
    If not, will show a red "FAILED" pop-up message.

    It returns void.
*/
void self_test(void *context, void *dummy1, void *dummy2) {
    init_ui_display();

#ifdef SIMULATE_LED_STRIP_INIT_FAILURE
    init_retcode.led_strip = -1;
#endif

    // ---- init
    bool init_result = true;
    init_result &= check_int_and_print(init_retcode.user_led, USER_LED_ROW, USER_LED_TEXT);
    init_result &= check_int_and_print(init_retcode.ambient_light, AMBIENT_ROW, AMBIENT_LIGHT_TEXT);
    init_result &= check_int_and_print(init_retcode.lsm6dsl, LSM6DSL_ROW, LSM6DSL_TEXT);
    init_result &= check_int_and_print(init_retcode.sht31, SHT31_ROW, SHT31_TEXT);
    init_result &= check_int_and_print(init_retcode.led_strip, LED_STRIP_ROW, WS2812_LEDS_TEXT);
    init_result &= check_int_and_print(init_retcode.display, DISPLAY_ROW, DISPLAY_TEXT);
    init_result &= check_int_and_print(init_retcode.expresslink, EXPRESSLINK_ROW, EXPRESSLINK_TEXT);
    init_result &= check_int_and_print(init_retcode.usb_mass_storage, USB_MASS_STORAGE_ROW, USB_MASS_STORAGE_TEXT);

    if (init_result == false) {
        show_popup("FAILED (init)", 0xff0000);
        k_msleep(10000);
        cleanup_ui_display();
        return;
    }

    // ---- self-tests
    bool test_result = true;
    test_result &= test_flash();
    test_result &= test_expresslink();
    test_result &= test_ambient_light();
    test_result &= test_lsm6dsl();
    test_result &= test_sht3xd();

    if (test_result == false) {
        show_popup("FAILED (tests)", 0xff0000);
        k_msleep(10000);
        cleanup_ui_display();
        return;
    }

    check_bool_and_print(true, BUTTONS_ROW, "Press B1", NULL);
    show_popup("Press Button 1!", 0xc0c0ff);

    k_msleep(100);
    display_handler();

    if (!wait_for_button(&button1_pressed)) {
        return;
    }
    check_bool_and_print(true, BUTTONS_ROW, "Press B2", NULL);
    show_popup("Press Button 2!", 0xa0a0ff);
    led_strip_set_pixel(0, 0, 0, 0x70);

    if (!wait_for_button(&button2_pressed)) {
        return;
    }
    check_bool_and_print(true, BUTTONS_ROW, "Press B3", NULL);
    show_popup("Press Button 3!", 0x9090ff);
    led_strip_set_pixel(1, 0, 0, 0x70);

    if (!wait_for_button(&button3_pressed)) {
        return;
    }
    check_bool_and_print(true, BUTTONS_ROW, "Press B4", NULL);
    show_popup("Press Button 4!", 0x7070ff);
    led_strip_set_pixel(2, 0, 0, 0x70);

    if (!wait_for_button(&button4_pressed)) {
        return;
    }

    check_bool_and_print(true, BUTTONS_ROW, "User Buttons", NULL);

    led_strip_set_pixel(0, 0, 0x70, 0);
    k_msleep(10); // needed to flush the led_strip buffer to the driver
    led_strip_set_pixel(1, 0, 0x70, 0);
    k_msleep(10); // needed to flush the led_strip buffer to the driver
    led_strip_set_pixel(2, 0, 0x70, 0);
    k_msleep(10); // needed to flush the led_strip buffer to the driver

    // reset expresslink to drive EL_EVENT high
    // --> EVENT LED should turn on
    test_expresslink_reset();

    // check EL_EVENT signal
    test_result = check_bool_and_print(expresslink_check_event_pending(), EL_EVENT_ROW, EL_EVENT_TEXT, NULL);

#ifdef SIMULATE_EL_EVENT_FAILURE
    test_result = false;
#endif

    if (test_result == false) {
        show_popup("FAILED (tests)", 0xff0000);
    } else {
        turn_user_led_on();
        show_popup("PASSED", 0x00ff00);
    }

    while (true) {
        if (shutdown_request_received()) {
            cleanup_ui_display();
            return;
        }
        k_msleep(10);
    }

    return;
}
