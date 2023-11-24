// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#if CONFIG_SIDEWALK

#include <assert.h>

#include <app_ble_config.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sidewalk_task);

#include "badge.h"
#include "sidewalk/sidewalk.h"
#include "sidewalk/sidewalk_ui_display.h"

#define CONFIG_SIDEWALK_SLEEP_TIME (30)

K_THREAD_STACK_DEFINE(sm_main_task_stack, CONFIG_SIDEWALK_THREAD_STACK_SIZE);
static struct k_thread sm_main_task;

#define RECEIVE_TASK_STACK_SIZE (4096)
#define RECEIVE_TASK_PRIORITY (CONFIG_SIDEWALK_THREAD_PRIORITY)
K_THREAD_STACK_DEFINE(sm_receive_task_stack, RECEIVE_TASK_STACK_SIZE);
static struct k_thread sm_receive_task;

K_MSGQ_DEFINE(sm_main_task_msgq, sizeof(enum event_type), CONFIG_SIDEWALK_THREAD_QUEUE_SIZE, 4);
K_MSGQ_DEFINE(sm_rx_task_msgq, sizeof(struct app_demo_rx_msg), CONFIG_SIDEWALK_THREAD_QUEUE_SIZE, 4);

static app_context_t g_app_context = {
    .sidewalk_state = STATE_SIDEWALK_INIT,
    .app_state = DEMO_BADGE_STATE_INIT,
    .link_status.time_sync_status = SID_STATUS_NO_TIME,
};

static void stack_start_timer_cb(struct k_timer *timer_id);
K_TIMER_DEFINE(stack_start_timer, stack_start_timer_cb, NULL);
void stack_start_timer_cb(struct k_timer *timer_id) {
    sm_main_task_msg_q_write(DEMO_BADGE_START);
}

static void stack_stop_timer_cb(struct k_timer *timer_id);
K_TIMER_DEFINE(stack_stop_timer, stack_stop_timer_cb, NULL);
void stack_stop_timer_cb(struct k_timer *timer_id) {
    sm_main_task_msg_q_write(DEMO_BADGE_STOP);
}

static int32_t init_and_start_link(app_context_t *context, struct sid_event_callbacks *event_callbacks) {
    struct sid_handle *sid_handle = NULL;

    LOG_INF("Initializing...");

    struct sid_config config = {
        .link_mask = SID_LINK_TYPE_1,
        .time_sync_periodicity_seconds = 7200,
        .callbacks = event_callbacks,
        .link_config = app_get_ble_config(),
        .sub_ghz_link_config = NULL,
    };

    sid_error_t ret = sid_init(&config, &sid_handle);
    if (ret != SID_ERROR_NONE) {
        LOG_ERR("sid_init failed: %d", (int)ret);
        return -1;
    }

    context->sidewalk_handle = sid_handle;

    LOG_INF("Starting...");
    ret = sid_start(sid_handle, config.link_mask);
    if (ret != SID_ERROR_NONE) {
        LOG_ERR("sid_start failed: %d", (int)ret);
        sid_deinit(context->sidewalk_handle);
        context->sidewalk_handle = NULL;
        return -1;
    }

	LOG_INF("Initialized and started.");
    return 0;
}

static void sidewalk_main_task(void *context, void *dummy1, void *dummy2) {
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);

    LOG_INF("Starting %s ...", __FUNCTION__);

    app_context_t *app_context = (app_context_t *)context;
    struct sid_event_callbacks event_callbacks;

    sid_error_t sid_ret = sm_callbacks_set(app_context, &event_callbacks);
    if (sid_ret != SID_ERROR_NONE) {
        LOG_ERR("sm_callbacks_set failed: %d", (int)sid_ret);
        return;
    }

    int ret = init_and_start_link(app_context, &event_callbacks);
    if (ret != 0) {
        return;
    }

    struct sid_handle *sid_handle = app_context->sidewalk_handle;
    app_context->sidewalk_state = STATE_SIDEWALK_NOT_READY;

    sidewalk_init_ui_display();

    k_timer_start(&stack_start_timer, K_SECONDS(CONFIG_SIDEWALK_SLEEP_TIME), K_SECONDS(CONFIG_SIDEWALK_SLEEP_TIME));

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'Sidewalk' (main task) module.");
            k_timer_stop(&stack_start_timer);
            k_timer_stop(&stack_stop_timer);
            sid_ret = sid_stop(sid_handle, SID_LINK_TYPE_1);
            if (sid_ret != SID_ERROR_NONE) {
                LOG_ERR("sid_stop failed: %d", (int)sid_ret);
            }
            sid_ret = sid_deinit(sid_handle);
            if (sid_ret != SID_ERROR_NONE) {
                LOG_ERR("sid_deinit failed: %d", (int)sid_ret);
            }
            sidewalk_cleanup_ui_display();
            return;
        }

        enum event_type event;
        int ret = k_msgq_get(&sm_main_task_msgq, &event, K_NO_WAIT);
        if (ret == -EAGAIN || ret == -ENOMSG) {
            // don't wait K_FOREVER to give the shutdown request a chance
            lv_label_set_text_fmt(sidewalk_label_last, "last updated %.1fs ago...", MAX(0, k_uptime_get() - sidewalk_last_updated_age) / 1000.0f);
            display_handler();
            k_msleep(100);
            continue;
        } else if (ret != 0) {
            LOG_WRN("unknown k_msgq_get error: %d", ret);
            continue;
        }

        switch (event) {
        case EVENT_TYPE_SIDEWALK: {
            sid_error_t ret = sid_process(sid_handle);
            if (ret == SID_ERROR_STOPPED) {
                LOG_INF("Connection stopped.");
            } else if (ret) {
                LOG_WRN("sid_process error: %d", ret);
            }
            break;
        }
        case DEMO_BADGE_START: {
            LOG_INF("Starting...");
            sid_ret = sid_start(sid_handle, SID_LINK_TYPE_1);
            if (sid_ret != SID_ERROR_NONE) {
                LOG_ERR("sid_start failed: %d", sid_ret);
            }
            LOG_INF("Beaconing via BLE to connect...");
			sid_error_t ret = sid_ble_bcn_connection_request(sid_handle, true);
			if (ret != SID_ERROR_NONE) {
				LOG_WRN("sid_ble_bcn_connection_request failed: %d - retrying BLE beaconing again later...", ret);
			}
            break;
        }
        case DEMO_BADGE_STOP: {
            LOG_INF("Going to sleep for %d seconds...", CONFIG_SIDEWALK_SLEEP_TIME);
            sid_error_t ret = sid_stop(sid_handle, SID_LINK_TYPE_1);
            if (ret != SID_ERROR_NONE) {
                LOG_ERR("sid_stop failed: %d", ret);
            }
            break;
        }
        default:
            LOG_ERR("Invalid event queued %d", event);
            break;
        }
    }
}

static void sidewalk_receive_task(void *context, void *dummy1, void *dummy2) {
    ARG_UNUSED(context);
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);

    LOG_INF("Starting %s ...", __FUNCTION__);

    while (true) {
        if (shutdown_request_received()) {
            LOG_INF("Shutting down 'Sidewalk' (receive task) module.");
            return;
        }

        struct app_demo_rx_msg rx_msg;
        int ret = k_msgq_get(&sm_rx_task_msgq, &rx_msg, K_NO_WAIT);
        if (ret == -EAGAIN || ret == -ENOMSG) {
            // don't wait K_FOREVER to give the shutdown request a chance
            k_msleep(10);
            continue;
        } else if (ret != 0) {
            LOG_WRN("unknown k_msgq_get error: %d", ret);
            continue;
        }

        LOG_INF("Received message from cloud: msg_id:%d, pld_size:%d", rx_msg.msg_id, rx_msg.pld_size);
        LOG_HEXDUMP_INF(rx_msg.rx_payload, rx_msg.pld_size, "sidewalk_received_msg_payload");
        // no further processing of incoming messages
    }
}

void sm_main_task_msg_q_write(enum event_type event) {
    while (k_msgq_put(&sm_main_task_msgq, &event, K_NO_WAIT)) {
        LOG_WRN("sm_main_task_msgq queue is full, purge old data");
        k_msgq_purge(&sm_main_task_msgq);
    }
}

void sm_rx_task_msg_q_write(struct app_demo_rx_msg *rx_msg) {
    while (k_msgq_put(&sm_rx_task_msgq, rx_msg, K_NO_WAIT)) {
        LOG_WRN("sm_rx_task_msgq queue is full, purge old data");
        k_msgq_purge(&sm_rx_task_msgq);
    }
}

void sm_send_msg(const app_context_t *app_context, struct sid_msg_desc *desc, struct sid_msg *msg) {
    assert(app_context);
    assert(desc);
    assert(msg);

    if (app_context->sidewalk_state != STATE_SIDEWALK_READY) {
        LOG_WRN("not ready yet to send message!");
        return;
    }

    sid_error_t ret = sid_put_msg(app_context->sidewalk_handle, msg, desc);
    if (ret != SID_ERROR_NONE) {
        k_msleep(500); // retry once after a bit of time
        sid_error_t ret = sid_put_msg(app_context->sidewalk_handle, msg, desc);
        if (ret != SID_ERROR_NONE) {
            LOG_WRN("sid_put_msg failed: %d - retrying again later...", (int)ret);
            return;
        }
    }
    LOG_INF("Queued message (id: %u).", desc->id);
}

void sm_task_start(void) {
    (void)k_thread_create(&sm_main_task,
                          sm_main_task_stack,
                          K_THREAD_STACK_SIZEOF(sm_main_task_stack),
                          sidewalk_main_task,
                          &g_app_context,
                          NULL,
                          NULL,
                          CONFIG_SIDEWALK_THREAD_PRIORITY,
                          0,
                          K_NO_WAIT);

    (void)k_thread_create(&sm_receive_task,
                          sm_receive_task_stack,
                          K_THREAD_STACK_SIZEOF(sm_receive_task_stack),
                          sidewalk_receive_task,
                          NULL,
                          NULL,
                          NULL,
                          RECEIVE_TASK_PRIORITY,
                          0,
                          K_NO_WAIT);

    k_thread_name_set(&sm_main_task, "sm_main_task");
    k_thread_name_set(&sm_receive_task, "sm_receive_task");
}
#endif
