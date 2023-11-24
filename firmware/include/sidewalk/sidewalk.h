// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SIDEWALK_H
#define SIDEWALK_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include <sid_api.h>

#define PAYLOAD_MAX_SIZE (255)

extern struct k_timer stack_start_timer;
extern struct k_timer stack_stop_timer;

struct app_demo_rx_msg {
	uint16_t msg_id;
	size_t pld_size;
	uint8_t rx_payload[PAYLOAD_MAX_SIZE];
};

enum event_type {
	EVENT_TYPE_SIDEWALK,
	EVENT_FACTORY_RESET,
	EVENT_CONNECT_LINK_TYPE_1,
	DEMO_BADGE_START,
	DEMO_BADGE_STOP,
};

struct link_status {
	enum sid_time_sync_status time_sync_status;
	uint32_t link_mask;
	uint32_t supported_link_mode[SID_LINK_TYPE_MAX_IDX];
};

enum sidewalk_state {
	STATE_SIDEWALK_INIT,
	STATE_SIDEWALK_READY,
	STATE_SIDEWALK_NOT_READY,
	STATE_SIDEWALK_SECURE_CONNECTION,
};

enum demo_badge_state {
	DEMO_BADGE_STATE_INIT,
	DEMO_BADGE_STATE_REGISTERED,
};

typedef struct app_context {
	struct sid_handle *sidewalk_handle;
	enum sidewalk_state sidewalk_state;
	enum demo_badge_state app_state;
	struct link_status link_status;
	uint8_t buffer[PAYLOAD_MAX_SIZE];
} app_context_t;

/**
 * @brief Function for starting demo tasks.
 *
 */
void sm_task_start(void);

/**
 * @brief Add message to sidewalk queue.
 *
 * @param app_context application context.
 * @param desc message descriptor.
 * @param msg message payload.
 */
void sm_send_msg(const app_context_t *app_context, struct sid_msg_desc *desc, struct sid_msg *msg);

/**
 * @brief This function sets callbacks which will be invoked Sidewalk events occurs.
 *
 * @param ctx current user context.
 * @param cb pointer to object which stores callbacks.
 * @return SID_ERROR_NONE when success or error code otherwise.
 */
sid_error_t sm_callbacks_set(void *ctx, struct sid_event_callbacks *cb);

/**
 * @brief Send notification from sensors.
 *
 * @param app_context application context.
 * @param button_pressed flag inform is notification comes from button event.
 */
void sm_notify_sensor_data(app_context_t *app_context, bool button_pressed);

/**
 * @brief Add message to message queue.
 *
 * @param event Sidewalk event.
 */
void sm_main_task_msg_q_write(enum event_type event);

/**
 * @brief Add message to message queue.
 *
 * @param rx_msg pointer to message.
 */
void sm_rx_task_msg_q_write(struct app_demo_rx_msg *rx_msg);

#endif // SIDEWALK_H
