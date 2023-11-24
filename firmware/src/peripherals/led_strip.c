// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
LOG_MODULE_REGISTER(led_strip);

#include "badge.h"
#include "self_test.h"

static const struct device *const strip = DEVICE_DT_GET(DT_ALIAS(neopixels));
#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(neopixels), chain_length)
struct led_rgb pixels[STRIP_NUM_PIXELS];
struct led_rgb pixels_dimmed[STRIP_NUM_PIXELS];
uint8_t brightness;

#define RGB(_r, _g, _b) \
    { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
    RGB(0x0a, 0x00, 0x00), /* red */
    RGB(0x00, 0x0a, 0x00), /* green */
    RGB(0x00, 0x00, 0x0a), /* blue */
};

struct led_rgb pixels[STRIP_NUM_PIXELS];

int init_led_strip(void) {
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip device %s is not ready", strip->name);
        return -1;
    }

    brightness = 20;
    memset(&pixels, 0x00, sizeof(pixels));
    memset(&pixels_dimmed, 0x00, sizeof(pixels));

    if (led_strip_update_rgb(strip, pixels_dimmed, STRIP_NUM_PIXELS) != 0) {
        LOG_ERR("failed to update led strip");
        return -1;
    }

    LOG_INF("init complete.");
    return 0;
}

/*
  @param value: 0 to 100 in percent
*/
void led_strip_set_brightness(uint8_t value) {
    if (value <= 100) {
        brightness = value;
    } else {
        brightness = 100;
    }
    led_strip_refresh();
}

/*
  @param index: 0, 1, or 2 for the LED index in the chain
  @param r: 0 to 255 for red
  @param g: 0 to 255 for green
  @param b: 0 to 255 for blue
*/
void led_strip_set_pixel(size_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index > STRIP_NUM_PIXELS - 1) {
        LOG_ERR("invalid index");
        return;
    }

    // index is reversed to JSON name to get a nice visual "top-down" numbering, instead of "bottom-up"
    switch (index) {
    case 0:
        index = 2;
        break;
    case 1:
        index = 1;
        break;
    case 2:
        index = 0;
        break;
    default:
        index = 2;
        break;
    }

    struct led_rgb value = RGB(r, g, b);
    memcpy(pixels + index, &value, sizeof(value));
    led_strip_refresh();
}

void led_strip_refresh() {
    for (size_t i = 0; i < STRIP_NUM_PIXELS; i++) {
        pixels_dimmed[i].r = (float)pixels[i].r * brightness / 100.0f;
        pixels_dimmed[i].g = (float)pixels[i].g * brightness / 100.0f;
        pixels_dimmed[i].b = (float)pixels[i].b * brightness / 100.0f;
    }
    if (led_strip_update_rgb(strip, pixels_dimmed, STRIP_NUM_PIXELS) != 0) {
        LOG_ERR("failed to update led strip");
    }
    k_msleep(1);
}

int run_led_strip(void) {
    size_t cursor = 0, color = 0;

    int i = 10;
    while (i--) {
        memset(&pixels, 0x00, sizeof(pixels));
        memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));

        if (led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS) != 0) {
            LOG_ERR("failed to update led strip");
            return -1;
        }

        cursor++;
        if (cursor >= STRIP_NUM_PIXELS) {
            cursor = 0;
            color++;
            if (color == ARRAY_SIZE(colors)) {
                color = 0;
            }
        }

        k_msleep(200);
    }
    return 0;
}
