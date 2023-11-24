#!/usr/bin/env bash

set -x

cd "$(dirname $BASH_SOURCE)"

arm-zephyr-eabi-gdb \
  --init-eval-command="set confirm off" \
  --init-eval-command="target extended-remote | openocd -f debug.cfg -c 'gdb_port pipe ; log_output build/openocd.log'" \
  --init-eval-command="add-symbol-file build/zephyr/zephyr.elf" \
  --init-eval-command="break k_sys_fatal_error_handler" \
  --init-eval-command="set confirm on"
