// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SELF_TEST_H
#define SELF_TEST_H

#include <zephyr/kernel.h>

int run_user_led(void);
int run_buttons(void);
int run_ambient_light(void);
int run_sht31(void);
int run_lsm6dsl(void);
int run_led_strip(void);
int run_expresslink(void);
int run_expresslink_debug(void);
int run_display(void);

#endif // SELF_TEST_H
