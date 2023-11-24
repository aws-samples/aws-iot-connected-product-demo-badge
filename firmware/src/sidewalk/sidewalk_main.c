// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#if CONFIG_SIDEWALK

#include <pal_init.h>
#include <sidewalk_version.h>

#include "badge.h"
#include "sidewalk/sidewalk.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sidewalk_main);

void start_sidewalk_sample(void *context, void *dummy1, void *dummy2) {
    int ret;

    expresslink_reset();
    expresslink_send_command("AT+EVENT?\n", NULL, 0); // clear out STARTUP event to turn off EVENT LED

    ret = init_sidewalk_mfg_payload();
    if (ret != 0) {
        LOG_ERR("Failed to init Sidewalk MFG payload: %d", ret);
        return;
    }

    ret = application_pal_init();
    if (ret) {
        LOG_ERR("Failed to initialize PAL layer: %d", ret);
        return;
    }

    sm_task_start();

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'Sidewalk' module.");
            return;
        }
        k_msleep(10);
    }
}
#endif
