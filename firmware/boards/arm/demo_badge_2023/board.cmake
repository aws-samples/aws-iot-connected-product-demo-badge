# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

board_runner_args(pyocd "--target=nrf52840" "--frequency=4000000")
set(OPENOCD_NRF5_SUBFAMILY "nrf52")

include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd-nrf5.board.cmake)
