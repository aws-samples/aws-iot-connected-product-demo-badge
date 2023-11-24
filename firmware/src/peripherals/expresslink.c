// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/ring_buffer.h>
LOG_MODULE_REGISTER(expresslink);

#include "badge.h"
#include "self_test.h"

K_MUTEX_DEFINE(uart_expresslink_mutex);
const struct device *uart_expresslink = DEVICE_DT_GET(DT_NODELABEL(uart0));

K_EVENT_DEFINE(uart_expresslink_tx_done);

RING_BUF_DECLARE(receive_ring, 4096);

#define UART_RX_ASYNC_TIMEOUT (10000)
#define RECV_BUF_LENGTH (4096)
static char *recv_buf0;
static char *recv_buf1;
static uint8_t active_recv_buf_id = 0;

const size_t response_line_length = RECV_BUF_LENGTH * 2;
static char *response_line;
static size_t response_offset = 0;

static const struct shell *passthrough_shell = NULL;
static bool passthrough_local_echo = true;

static struct gpio_callback event_interrupt_cb_data;

#define passthrough_TASK_PRIORITY 10
K_THREAD_STACK_DEFINE(passthrough_task_stack, 1024);
static struct k_thread passthrough_task;

// const struct device *uart_expresslink_debug = DEVICE_DT_GET(DT_NODELABEL(uart1));
// char recv_buf_debug[256];

struct uart_config uart_cfg = {
    .baudrate = 115200,
    .parity = UART_CFG_PARITY_NONE,
    .stop_bits = UART_CFG_STOP_BITS_1,
    .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    .data_bits = UART_CFG_DATA_BITS_8,
};

static const struct gpio_dt_spec expresslink_event_pin = GPIO_DT_SPEC_GET(DT_ALIAS(expresslink_event), gpios);
static const struct gpio_dt_spec expresslink_wake_pin = GPIO_DT_SPEC_GET(DT_ALIAS(expresslink_wake), gpios);
static const struct gpio_dt_spec expresslink_reset_pin = GPIO_DT_SPEC_GET(DT_ALIAS(expresslink_reset), gpios);

bool expresslink_check_event_pending(void) {
    return gpio_pin_get_dt(&expresslink_event_pin) == 1;
}

bool expresslink_is_event(const char *response, const char *event) {
    return strncmp(response, event, strlen(event)) == 0;
}

static void blocking_uart_expresslink_tx(const uint8_t *buf, size_t len) {
    int ret = uart_tx(uart_expresslink, buf, len, SYS_FOREVER_US);
    if (ret != 0) {
        LOG_ERR("ExpressLink uart_tx failed: %d", ret);
        return;
    }

    int events = k_event_wait(&uart_expresslink_tx_done, 0xF, true, K_MSEC(10000));
    if (events == 0) {
        LOG_ERR("ExpressLink: uart_tx timeout, last message to send was: %.*s", len, buf);
    }
}

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data) {
    int ret;

    switch (evt->type) {
    case UART_TX_DONE:
        k_event_set(&uart_expresslink_tx_done, 0x1);
        break;
    case UART_TX_ABORTED:
        LOG_ERR("UART UART_TX_ABORTED");
        break;
    case UART_RX_RDY:
        size_t rb_len = ring_buf_put(&receive_ring, evt->data.rx.buf + evt->data.rx.offset, evt->data.rx.len);
        if (rb_len < evt->data.rx.len) {
            LOG_ERR("UART ring: Drop %u bytes", evt->data.rx.len - rb_len);
        }
        // LOG_INF("UART UART_RX_RDY: buf:%p offset:%d length:%d copy complete.", evt->data.rx.buf, evt->data.rx.offset, evt->data.rx.len);
        break;
    case UART_RX_BUF_REQUEST:
        char *new_buf;
        if (active_recv_buf_id == 0) {
            active_recv_buf_id = 1;
            new_buf = recv_buf1;
        } else {
            active_recv_buf_id = 0;
            new_buf = recv_buf0;
        }
        // LOG_INF("UART UART_RX_BUF_REQUEST - responding with new buffer: slot %d at %p", active_recv_buf_id, new_buf);
        uart_rx_buf_rsp(uart_expresslink, new_buf, RECV_BUF_LENGTH);
        break;
    case UART_RX_BUF_RELEASED:
        // LOG_INF("UART UART_RX_BUF_RELEASED released buffer: %p", evt->data.rx_buf.buf);
        break;
    case UART_RX_DISABLED:
        LOG_INF("UART UART_RX_DISABLED");
        active_recv_buf_id = 0;
        ret = uart_rx_enable(uart_expresslink, recv_buf0, RECV_BUF_LENGTH, UART_RX_ASYNC_TIMEOUT);
        if (ret != 0) {
            LOG_ERR("re-enabling uart_rx_enable failed: %d", ret);
        }
        break;
    case UART_RX_STOPPED:
        LOG_ERR("UART UART_RX_STOPPED %d", evt->data.rx_stop.reason);
        break;
    default:
        LOG_WRN("UART CB unhandled event! %d", evt->type);
        break;
    }
}

char *readline(const struct device *uart) {
    response_offset = 0;

    // No command can take more than 120 seconds to complete, according to ExpressLink spec v1.1.2
    // https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-responses
    int64_t deadline = k_uptime_get() + 120 * 1000;

    while (k_uptime_get() < deadline) {
        if (ring_buf_get(&receive_ring, response_line + response_offset, 1) != 1) {
            k_msleep(10);
            continue;
        }

        if (response_line[response_offset] == '\n') {
            response_line[response_offset] = 0;
            if (response_offset > 1 && response_line[response_offset - 1] == '\r') {
                response_line[response_offset - 1] = 0;
            }
            return response_line;
        }
        response_offset++;
    }
    LOG_WRN("UART timeout!");
    ring_buf_reset(&receive_ring);
    return "";
}

bool expresslink_send_command(const char *command, char *response, size_t response_length) {
    k_mutex_lock(&uart_expresslink_mutex, K_FOREVER);

    // WARNING: do not log the full command - it might be too long and crash the logging subsystem!

    size_t command_length = strlen(command);

    if (command[command_length - 1] != '\n') {
        LOG_WRN("command missing newline! %s", command + command_length - 5);
    }

    const size_t max_log_cmd_length = 128;
    if (command_length < max_log_cmd_length) {
        size_t len = (command[command_length - 1] != '\n') ? (command_length) : (command_length - 1);
        LOG_INF("> %.*s", len, command);
    } else {
        char log_msg[max_log_cmd_length];
        snprintf(log_msg, sizeof(log_msg), "%s", command);
        LOG_INF("> %s [... command too long ...]", log_msg);
    }

    const char intercept_cmd[] = "AT+DIAG WIFI SCAN";
    const char magic_arg[] = " workshop ";
    if (strncmp(command, intercept_cmd, strlen(intercept_cmd)) == 0 && strstr(command, magic_arg) != NULL) {
        if (workshop_wifi_device_location_override(response_line, response_line_length)) {
            snprintf(response, response_length, "%s", response_line + 3);
            LOG_INF("< %s", response_line);
            k_mutex_unlock(&uart_expresslink_mutex);
            return true;
        }
    }

    blocking_uart_expresslink_tx(command, command_length);

    char *r = readline(uart_expresslink);

    const char always_log_response_cmd[] = "AT+CONF? ";
    const size_t max_log_response_length = 62;
    if (strncmp(command, always_log_response_cmd, strlen(always_log_response_cmd)) == 0 || strlen(r) < max_log_response_length) {
        LOG_INF("< %s", r);
    } else {
        char log_msg[max_log_response_length];
        snprintf(log_msg, sizeof(log_msg), "%s", r);
        LOG_INF("< %s [... response too long ...]", log_msg);
    }

    bool success = false;

    if (strcmp(r, "OK") == 0 && (r[2] == 0x0 || r[2] == '\n' || r[2] == '\r')) {
        // OK\r\n
        success = true;
        if (response != NULL) {
            *response = 0;
        }
    } else if (strncmp(r, "OK ", 3) == 0) {
        // OK Some Message
        success = true;
        if (response != NULL) {
            snprintf(response, response_length, "%s", r + 3);
        }
    } else if (strncmp(r, "OK", 2) == 0 && isdigit((int)r[2])) {
        // OK1 Some Message
        // And one additional line
        success = true;
        if (response != NULL) {
            snprintf(response, response_length, "%s", r + 2);
        }
    } else {
        success = false;
        if (response != NULL) {
            snprintf(response, response_length, "%s", r);
        }
    }

    k_mutex_unlock(&uart_expresslink_mutex);
    return success;
}

int expresslink_read_response_line(char *buffer, size_t buffer_length) {
    char *r = readline(uart_expresslink);
    snprintf(buffer, buffer_length, "%s", r);
    return 0;
}

void event_interrupt_cb_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    LOG_DBG("EVENT interrupt triggered!");
}

static int init_buffers() {
    recv_buf0 = k_malloc(RECV_BUF_LENGTH);
    if (recv_buf0 == NULL) {
        LOG_ERR("k_malloc failed for recv_buf0!");
        return -1;
    }
    recv_buf1 = k_malloc(RECV_BUF_LENGTH);
    if (recv_buf1 == NULL) {
        LOG_ERR("k_malloc failed recv_buf1!");
        return -1;
    }
    response_line = k_malloc(response_line_length);
    if (response_line == NULL) {
        LOG_ERR("k_malloc failed response_line!");
        return -1;
    }
    return 0;
}

static int init_uart() {
    int ret = uart_configure(uart_expresslink, &uart_cfg);
    if (ret) {
        LOG_ERR("Could not configure device %s", uart_expresslink->name);
        return -1;
    }
    ret = uart_callback_set(uart_expresslink, uart_cb, NULL);
    if (ret != 0) {
        LOG_ERR("uart_callback_set failed");
        return -1;
    }

    active_recv_buf_id = 0;
    ret = uart_rx_enable(uart_expresslink, recv_buf0, RECV_BUF_LENGTH, UART_RX_ASYNC_TIMEOUT);
    if (ret != 0) {
        LOG_ERR("uart_rx_enable failed: %d", ret);
        return -1;
    }

    return 0;
}

static int init_pins() {
    if (!gpio_is_ready_dt(&expresslink_event_pin)) {
        LOG_ERR("expresslink_event device not ready, aborting test");
        return -1;
    }
    if (!gpio_is_ready_dt(&expresslink_wake_pin)) {
        LOG_ERR("expresslink_event device not ready, aborting test");
        return -1;
    }
    if (!gpio_is_ready_dt(&expresslink_reset_pin)) {
        LOG_ERR("expresslink_event device not ready, aborting test");
        return -1;
    }

    if (gpio_pin_configure_dt(&expresslink_event_pin, GPIO_INPUT) != 0) {
        LOG_ERR("expresslink_event gpio pin configure failed, aborting test");
        return -1;
    }
    if (gpio_pin_configure_dt(&expresslink_wake_pin, GPIO_OUTPUT_LOW) != 0) {
        LOG_ERR("expresslink_wake gpio pin configure failed, aborting test");
        return -1;
    }
    if (gpio_pin_configure_dt(&expresslink_reset_pin, GPIO_OUTPUT_LOW) != 0) {
        LOG_ERR("expresslink_reset gpio pin configure failed, aborting test");
        return -1;
    }
    return 0;
}

static int init_event_interrupt() {
    if (gpio_pin_interrupt_configure_dt(&expresslink_event_pin, GPIO_INT_EDGE_TO_ACTIVE) != 0) {
        LOG_ERR("expresslink_event gpio pin interrupt configure failed, aborting test");
        return -1;
    }

    gpio_init_callback(&event_interrupt_cb_data, event_interrupt_cb_handler, BIT(expresslink_event_pin.pin));

    if (gpio_add_callback(expresslink_event_pin.port, &event_interrupt_cb_data) != 0) {
        LOG_ERR("expresslink_event gpio add callback failed");
        return -1;
    }

    return 0;
}

int init_expresslink(void) {
    int ret;
    ret = init_buffers();
    if (ret != 0) {
        return ret;
    }

    ret = init_uart();
    if (ret != 0) {
        return ret;
    }

    ret = init_pins();
    if (ret != 0) {
        return ret;
    }

    expresslink_reset();
    expresslink_wake();

    expresslink_send_command("AT+EVENT?\n", NULL, 0); // read in the STARTUP event to get rid of EVENT LED

    ret = init_event_interrupt();
    if (ret != 0) {
        return ret;
    }

    LOG_INF("init complete.");
    return 0;
}

int expresslink_over_the_wire_update(const char *path, const char *expected_version, bool force_update) {
    int ret;

    char version[64] = {0};
    expresslink_send_command("AT+CONF? Version\n", version, sizeof(version));
    if (expected_version != NULL && strncmp(version, expected_version, sizeof(version)) == 0 && !force_update) {
        return 0;
    }

    char abs_path[64];
    char prefix[] = USB_PATH();
    if (strncmp(prefix, path, strlen(prefix)) != 0) {
        snprintf(abs_path, sizeof(abs_path), "%s%s", prefix, path);
    } else {
        snprintf(abs_path, sizeof(abs_path), "%s", path);
    }

    LOG_INF("Updating from file: %s", abs_path);

    struct fs_dirent dirent;
    ret = fs_stat(abs_path, &dirent);
    if (ret != 0) {
        LOG_ERR("File not found or invalid %s: %d", abs_path, ret);
        return -1;
    }

    if (dirent.size == 0 || dirent.size > 4 * 1024 * 1024) {
        // ESP32-C3-MINI-1-N4-A has 4MB internal flash - so the maximum firmware size must be smaller
        LOG_ERR("ExpressLink firmware file unexpected size: %d bytes", dirent.size);
        return -1;
    }

    struct fs_file_t file;
    fs_file_t_init(&file);
    ret = fs_open(&file, abs_path, FS_O_READ);
    if (ret) {
        LOG_ERR("Failed to open %s: %d", abs_path, ret);
        return -1;
    }

    k_mutex_lock(&uart_expresslink_mutex, K_FOREVER);

    LOG_INF("Current ExpressLink information before update:");
    expresslink_send_command("AT+CONF? About\n", NULL, 0);
    expresslink_send_command("AT+CONF? Version\n", NULL, 0);
    expresslink_send_command("AT+CONF? TechSpec\n", NULL, 0);

    size_t block_size = 2048;
    size_t buf_size = 128;
    uint8_t *buf = k_malloc(buf_size);

    snprintf(buf, buf_size, "AT+OTW %u,%u\n", dirent.size, block_size);
    expresslink_send_command(buf, NULL, 0);

    size_t count = 0;
    while (true) {
        size_t read = fs_read(&file, buf, buf_size);
        if (read == 0) {
            // end of file reached
            break;
        }
        count += read;

        blocking_uart_expresslink_tx(buf, read);

        if (count % block_size == 0) {
            char *r = readline(uart_expresslink);
            if (strncmp(r, "OK", 2) != 0) {
                LOG_ERR("ExpressLink firmeware update failed - unexpected response: %s", r);
                ret = -1;
                goto cleanup;
            }
        }
        if (count % (block_size * 2) == 0) {
            toggle_user_led();
        }
        if (count % (block_size * 4) == 0) {
            float percentage = (float)count / (float)dirent.size * 100.0f;
            LOG_INF("ExpressLink firmware update progress: %.1f...", percentage);

            led_strip_set_brightness(20);
            if (percentage < 33.3f) {
                led_strip_set_pixel(0, 0, (uint8_t)(255.0f * percentage / 33.3f), 0);
                led_strip_set_pixel(1, 0, 0, 0);
                led_strip_set_pixel(2, 0, 0, 0);
            } else if (percentage < 66.6f) {
                led_strip_set_pixel(0, 0, 255, 0);
                led_strip_set_pixel(1, 0, (uint8_t)(255.0f * (percentage - 33.3f) / 33.3f), 0);
                led_strip_set_pixel(2, 0, 0, 0);
            } else {
                led_strip_set_pixel(0, 0, 255, 0);
                led_strip_set_pixel(1, 0, 255, 0);
                led_strip_set_pixel(2, 0, (uint8_t)(255.0f * (percentage - 66.6f) / 33.3f), 0);
            }
        }
    }
    k_msleep(500);

    char *r = readline(uart_expresslink);
    const char *completion_msg = "OK COMPLETE";
    if (strncmp(r, completion_msg, strlen(completion_msg)) != 0) {
        LOG_ERR("ExpressLink firmeware update failed - unexpected response: %s", r);
        ret = -1;
        goto cleanup;
    }
    LOG_INF("%s", r);

    expresslink_send_command("AT+RESET\n", NULL, 0);
    k_msleep(3000);

    LOG_INF("New ExpressLink information after update:");
    expresslink_send_command("AT+CONF? About\n", NULL, 0);
    expresslink_send_command("AT+CONF? Version\n", NULL, 0);
    expresslink_send_command("AT+CONF? TechSpec\n", NULL, 0);

    LOG_INF("ExpressLink firmware update completed.");
    turn_user_led_off();
    led_strip_set_pixel(0, 0, 0, 0);
    led_strip_set_pixel(1, 0, 0, 0);
    led_strip_set_pixel(2, 0, 0, 0);

    ret = 0;

cleanup:
    fs_close(&file);
    k_mutex_unlock(&uart_expresslink_mutex);
    return ret;
}

void expresslink_reset() {
    gpio_pin_set_dt(&expresslink_reset_pin, 0); // active LOW
    k_msleep(50);
    ring_buf_reset(&receive_ring);
    gpio_pin_set_dt(&expresslink_reset_pin, 1);
    k_msleep(2500); // give it time to boot up
}

void expresslink_wake() {
    gpio_pin_set_dt(&expresslink_wake_pin, 0); // active LOW
    k_msleep(50);
    gpio_pin_set_dt(&expresslink_wake_pin, 1);
}

int expresslink_export_certificate(const struct shell *sh, size_t argc, char **argv, bool force_write) {
    int ret;
    struct fs_file_t file;
    fs_file_t_init(&file);

    char thing_name[64];
    expresslink_send_command("AT+CONF? ThingName\n", thing_name, sizeof(thing_name));

    char path[128];
    char fmt[] = USB_PATH("expresslink_certificate_%s.pem");
    snprintf(path, sizeof(path), fmt, thing_name);

    struct fs_dirent dirent;
    ret = fs_stat(path, &dirent);
    if (ret == 0 && dirent.size > 1000 && !force_write) {
        return 0;
    }

    ret = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE);
    if (ret != 0) {
        LOG_WRN("fs_open failed: %d", ret);
        return -1;
    }
    ret = fs_truncate(&file, 0);
    if (ret != 0) {
        LOG_WRN("fs_truncate failed: %d", ret);
        return -1;
    }

    const size_t expresslink_response_length = 80; // fits at least PEM-formatted certificate line
    char expresslink_response[expresslink_response_length];
    bool success = expresslink_send_command("AT+CONF? Certificate pem\n", expresslink_response, expresslink_response_length);
    if (success && isdigit((int)expresslink_response[0])) {
        size_t additional_lines = atoi(expresslink_response);
        for (size_t i = 0; i < additional_lines; i++) {
            expresslink_read_response_line(expresslink_response, expresslink_response_length);
            shell_print(sh, "%s", expresslink_response);
            ret = fs_write(&file, expresslink_response, strlen(expresslink_response));
            if (ret < 0) {
                LOG_WRN("fs_write failed: %d", ret);
                return -1;
            }
            ret = fs_write(&file, "\r\n", 2); // be nice to Windows users
            if (ret < 0) {
                LOG_WRN("fs_write failed: %d", ret);
                return -1;
            }
        }
    }

    fs_close(&file);

    shell_print(sh, "");
    shell_print(sh, "Certificate successfully exported to USB mass storage: %s", path);

    return 0;
}

static int cmd_cmd(const struct shell *sh, size_t argc, char **argv) {
    k_mutex_lock(&uart_expresslink_mutex, K_FOREVER);

    // expresslink cmd AT+DIAG WIFI SCAN
    if (argc >= 5 && strcmp(argv[1], "AT+DIAG") == 0 && strcmp(argv[2], "WIFI") == 0 && strcmp(argv[3], "SCAN") == 0 && strcmp(argv[4], "workshop") == 0) {
        if (workshop_wifi_device_location_override(response_line, RECV_BUF_LENGTH)) {
            shell_print(sh, "%s", response_line);
            k_mutex_unlock(&uart_expresslink_mutex);
            return 0;
        }
    }

    // skip argv[0] as it just contains the "cmd" command name
    for (size_t i = 1; i < argc; i++) {
        blocking_uart_expresslink_tx(argv[i], strlen(argv[i]));

        bool eol_or_space = (i + 1 < argc);
        blocking_uart_expresslink_tx(eol_or_space ? " " : "\n", 1);
    }
    char *response = readline(uart_expresslink);
    shell_print(sh, "%s", response);

    if (strncmp(response, "OK", 2) == 0 && isdigit((int)response[2])) {
        size_t additional_lines = atoi(response + 2);
        for (size_t i = 0; i < additional_lines; i++) {
            char *response = readline(uart_expresslink);
            shell_print(sh, "%s", response);
        }
    }

    k_mutex_unlock(&uart_expresslink_mutex);
    return 0;
}

static void bypass_cb(const struct shell *sh, uint8_t *data, size_t len) {
    if (passthrough_local_echo) {
        for (size_t i = 0; i < len; i++) {
            uint8_t d = data[i];
            if (d == '\r') {
                shell_fprintf(sh, SHELL_VT100_COLOR_DEFAULT, "\r\n");
            } else {
                shell_fprintf(sh, SHELL_VT100_COLOR_DEFAULT, "%c", d);
            }
        }
    }

    const char *exit_cmd = "AT+EXIT";
    static uint8_t exit_command_state = 0;
    for (size_t i = 0; i < len; i++) {
        if (exit_command_state == strlen(exit_cmd) && (data[i] == '\r' || data[i] == '\n')) {
            shell_set_bypass(sh, NULL);
            k_thread_abort(&passthrough_task);
            ring_buf_reset(&receive_ring);
            passthrough_shell = NULL;
            shell_print(sh, "\n\nExiting Passthrough mode. Normal shell functionality restored.");
            k_mutex_unlock(&uart_expresslink_mutex);
            return;
        } else if (data[i] == exit_cmd[exit_command_state]) {
            exit_command_state++;
        } else {
            exit_command_state = 0;
        }
    }

    // send data to ExpressLink
    blocking_uart_expresslink_tx(data, len);
}

void passthrough_rx_loop(void *context, void *dummy1, void *dummy2) {
    while (true) {
        char c;
        if (ring_buf_get(&receive_ring, &c, 1) != 1) {
            k_msleep(1);
            continue;
        }
        shell_fprintf(passthrough_shell, SHELL_VT100_COLOR_DEFAULT, "%c", c);
    }
    LOG_WRN("passthrough rx loop ended.");
}

static int cmd_passthrough(const struct shell *sh, size_t argc, char **argv) {
    k_mutex_lock(&uart_expresslink_mutex, K_FOREVER);

    shell_print(sh, "Entering UART Passthrough mode for ExpressLink! Type `AT+EXIT<Enter>` to restore normal shell functionality.");
    if (argc == 2 && shell_check_arg_falsy(argv[1])) {
        passthrough_local_echo = false;
        shell_print(sh, "There is NO local echoing of input!");
    } else {
        passthrough_local_echo = true;
        shell_print(sh, "Local input will be echoed.");
    }

    passthrough_shell = sh;
    shell_set_bypass(sh, bypass_cb);

    k_thread_create(
        &passthrough_task,
        passthrough_task_stack,
        K_THREAD_STACK_SIZEOF(passthrough_task_stack),
        passthrough_rx_loop,
        NULL,
        NULL,
        NULL,
        passthrough_TASK_PRIORITY,
        0,
        K_NO_WAIT);
    k_thread_name_set(&passthrough_task, "passthrough_task");

    return 0;
}

static int cmd_wake(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    expresslink_wake();

    shell_print(sh, "ExpressLink WAKE performed.");
    return 0;
}

static int cmd_reset(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    expresslink_reset();

    shell_print(sh, "ExpressLink RESET performed.");
    return 0;
}

static int cmd_event(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (expresslink_check_event_pending()) {
        shell_print(sh, "ExpressLink EVENT is set! There are pending events!");
    } else {
        shell_print(sh, "ExpressLink EVENT is not set. There are no pending events.");
    }

    return 0;
}

static int cmd_debug(const struct shell *sh, size_t argc, char **argv) {
    bool s = false;
    if (argc == 2 && shell_check_arg_truthy(argv[1])) {
        s = true;
    }

    LOG_WRN("TODO pipe around data from uart_expresslink_debug");

    shell_print(sh, "ExpressLink debug logging %s.", s ? "enabled" : "disabled");
    return 0;
}

static int cmd_get_info(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    char version[128] = {0};
    char about_vendor_model[128] = {0};
    char tech_spec[128] = {0};
    expresslink_send_command("AT+CONF? Version\n", version, sizeof(version));
    expresslink_send_command("AT+CONF? About\n", about_vendor_model, sizeof(about_vendor_model));
    expresslink_send_command("AT+CONF? TechSpec\n", tech_spec, sizeof(tech_spec));

    shell_print(sh, "ExpressLink info:");
    shell_print(sh, "- Vendor / model: %s", about_vendor_model);
    shell_print(sh, "- FW version: %s", version);
    shell_print(sh, "- TechSpec version: %s", tech_spec);
    return 0;
}

static int cmd_update(const struct shell *sh, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    expresslink_over_the_wire_update(argv[1], NULL, true);
    return 0;
}

static int cmd_export_certificate(const struct shell *sh, size_t argc, char **argv) {
    return expresslink_export_certificate(sh, argc, argv, true);
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_expresslink,
	SHELL_CMD_ARG(cmd, NULL, "Send AT command", cmd_cmd, 2, 99), // allow sufficient optional arguments to avoid having to parse it as single arg with quotes
	SHELL_CMD_ARG(wake, NULL, "Wake up the ExpressLink module via the WAKE pin", cmd_wake, 1, 0),
	SHELL_CMD_ARG(reset, NULL, "Reset the ExpressLink module via the RESET pin", cmd_reset, 1, 0),
	SHELL_CMD_ARG(event, NULL, "Report the ExpressLink module EVENT pin status", cmd_event, 1, 0),
	SHELL_CMD_ARG(debug, NULL, "Set ExpressLink debug logging (0/1 or false/true)", cmd_debug, 2, 0),
	SHELL_CMD_ARG(info, NULL, "Get ExpressLink module info", cmd_get_info, 1, 0),
	SHELL_CMD_ARG(update, NULL, "Over-The-Wire update of ExpressLink firmware file, e.g., `v2.4.4.bin`", cmd_update, 2, 0),
	SHELL_CMD_ARG(export_certificate, NULL, "Export the certificate as PEM file to the USB mass storage device", cmd_export_certificate, 1, 0),
    SHELL_CMD_ARG(passthrough, NULL, "Enters a UART-passthrough mode with the ExpressLink module (local echo on by default, pass any argument to disable).", cmd_passthrough, 1, 1),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(expresslink, &sub_expresslink, "MQTT and AWS IoT ExpressLink commands", NULL);
