// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(primary_thread);

#include "badge.h"

// use a single thread to run different workshop modules to safe RAM for statically allocated thread stack
#define primary_thread_TASK_PRIORITY 10
K_THREAD_STACK_DEFINE(primary_thread_stack, 4096);
static struct k_thread primary_thread = {0};
static k_tid_t primary_thread_id = NULL;

struct k_event shutdown_request_event;

int init_primary_thread() {
    k_event_init(&shutdown_request_event);
    return 0;
}

bool shutdown_request_received() {
    return k_event_wait(&shutdown_request_event, 0x1, false, K_NO_WAIT) != 0;
}

void gracefully_shutdown_primary_thread() {
    // notify running thread to gracefully shutdown
    LOG_DBG("posting event");
    k_event_post(&shutdown_request_event, 0x1);

    // wait for it to gracefully terminate (returns immediately if not running)
    LOG_DBG("joining thread");
    if (primary_thread.base.thread_state != 0) {
        k_thread_join(&primary_thread, K_FOREVER);
        primary_thread_id = NULL;
    }

    // give all threads a chance to receive the event
    k_msleep(500);
}

void run(const char *name, void *function) {
    run_args(name, function, true, NULL, NULL, NULL);
}

void run_without_storing(const char *name, void *function) {
    run_args(name, function, false, NULL, NULL, NULL);
}

void run_args(const char *name, void *function, bool store, void *p1, void *p2, void *p3) {
    LOG_DBG("thread state: %d", primary_thread.base.thread_state);

    const char *thread_name = k_thread_name_get(primary_thread_id);
    if (primary_thread_id != NULL && thread_name != NULL && strcmp(thread_name, name) == 0) {
        LOG_WRN("%s is already running!", thread_name);
        return;
    }

    gracefully_shutdown_primary_thread();

    // reset the event
    LOG_DBG("clearing event");
    k_event_clear(&shutdown_request_event, 0x1);

    // persist workshop module if needed
    if (store) {
        store_active_workshop_module(name);
    }

    // reset all button states
    button1_pressed = false;
    button2_pressed = false;
    button3_pressed = false;
    button4_pressed = false;

    // start thread again with new name and function
    LOG_DBG("creating thread");
    LOG_INF("Running %s...", name); // this message is parsed in the Companion Web App and needs to match exactly!
    primary_thread_id = k_thread_create(
        &primary_thread,
        primary_thread_stack,
        K_THREAD_STACK_SIZEOF(primary_thread_stack),
        function,
        p1,
        p2,
        p3,
        primary_thread_TASK_PRIORITY,
        0,
        K_NO_WAIT);
    k_thread_name_set(&primary_thread, name);
}
