// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#define NRF_LOG_ENABLED 1
#define NRF_LOG_BACKEND_UART_ENABLED 1
#define NRF_LOG_BACKEND_SERIAL_USES_RTT 0

#include <time.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include "badge.h"

init_retcode_t init_retcode;

int main(void) {
    init_retcode.primary_thread = init_primary_thread();
    init_retcode.user_led = init_user_led();
    init_retcode.led_strip = init_led_strip();
    init_retcode.buttons = init_buttons();
    init_retcode.ambient_light = init_ambient_light();
    init_retcode.sht31 = init_sht31();
    init_retcode.lsm6dsl = init_lsm6dsl();
    init_retcode.expresslink = init_expresslink();
    init_retcode.display = init_display();
    init_retcode.usb_mass_storage = init_usb_mass_storage();

    k_msleep(1000);
    restore_active_workshop_module();

    return 0;
}
