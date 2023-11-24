// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef EXPRESSLINK_H
#define EXPRESSLINK_H

#include <stdbool.h>

// https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-event-handling.html#elpg-event-handling-commands
#define EL_EVENT_MSG                "1 "  // parameter = topic index. A message was received on topic #.
#define EL_EVENT_STARTUP            "2 "  // parameter = 0. The module has entered the active state.
#define EL_EVENT_CONLOST            "3 "  // parameter = 0. Connection unexpectedly lost.
#define EL_EVENT_OVERRUN            "4 "  // parameter = 0. Receive buffer Overrun (topic in detail).
#define EL_EVENT_OTA                "5 "  // parameter = 0. OTA event (see OTA? command for details).
#define EL_EVENT_CONNECT            "6 "  // parameter = Connection Hint. Connection established (== 0) or failed (> 0).
#define EL_EVENT_CONFMODE           "7 "  // parameter = 0. CONFMODE exit with success.
#define EL_EVENT_SUBACK             "8 "  // parameter = Topic Index. Subscription accepted.
#define EL_EVENT_SUBNACK            "9 "  // parameter = Topic Index. Subscription rejected.
#define EL_EVENT_PUBACK             "10"  // parameter = Topic Index. QoS1 PUBACK was received.
// 11..19 RESERVED
#define EL_EVENT_SHADOW_INIT        "20 " // parameter = Shadow Index. Shadow initialization successfully.
#define EL_EVENT_SHADOW_INIT_FAILED "21 " // parameter = Shadow Index. Shadow initialization failed.
#define EL_EVENT_SHADOW_DOC         "22 " // parameter = Shadow Index. Shadow document received.
#define EL_EVENT_SHADOW_UPDATE      "23 " // parameter = Shadow Index. Shadow update result received.
#define EL_EVENT_SHADOW_DELTA       "24 " // parameter = Shadow Index. Shadow delta update received.
#define EL_EVENT_SHADOW_DELETE      "25 " // parameter = Shadow Index. Shadow delete result received
#define EL_EVENT_SHADOW_SUBACK      "26 " // parameter = Shadow Index. Shadow delta subscription accepted.
#define EL_EVENT_SHADOW_SUBNACK     "27 " // parameter = Shadow Index. Shadow delta subscription rejected.
// 28..39 RESERVED
#define EL_EVENT_BLE_CONNECTED             "40 " // parameter = 0. BLE Connection was established peripheral role.
#define EL_EVENT_BLE_DISCOVER_COMPLETE     "41 " // parameter = 0 or Hint Code. 0 for successful; >0 vendor defined Hint Codes.
#define EL_EVENT_BLE_CONNECTION_LOST       "42 " // parameter = 0 or Central Index. Connection was terminated or 0 if peripheral role.
#define EL_EVENT_BLE_SUBSCRIBE_START       "43 " // parameter = GATT Index. Subscription started on BLEGATT# while on peripheral mode.
#define EL_EVENT_BLE_SUBSCRIBE_STOP        "44 " // parameter = GATT Index. Subscription terminated on BLEGATT# while on peripheral mode.
#define EL_EVENT_BLE_READ_REQUEST          "45 " // parameter = GATT Index. Read operation requested at BLEGATT# while on peripheral mode.
#define EL_EVENT_BLE_WRITE_REQUEST         "46 " // parameter = GATT Index. Write operation requested at BLEGATT# while on peripheral mode.
#define EL_EVENT_BLE_SUBSCRIPTION_RECEIVED "47 " // parameter = Subscription Index. Subscription was received on BLECentral# connection.
// <= 999 RESERVED

void expresslink_reset(void);
void expresslink_wake(void);

bool expresslink_check_event_pending(void);
bool expresslink_is_event(const char*, const char*);

bool expresslink_send_command(const char *command, char *response, size_t response_length);
int expresslink_read_response_line(char *buffer, size_t buffer_length);

void expresslink_provisioning(void);
int expresslink_export_certificate(const struct shell *sh, size_t argc, char **argv, bool force_write);
int expresslink_over_the_wire_update(const char *path, const char *expected_version, bool force_update);

#endif // EXPRESSLINK_H
