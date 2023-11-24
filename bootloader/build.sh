#!/usr/bin/env bash

# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

set -o nounset
set -o pipefail
set -o xtrace
set -o errexit

cd "$(dirname ${BASH_SOURCE})"

rm -rf Adafruit_nRF52_Bootloader
git clone --depth 1 --branch 0.8.0 https://github.com/adafruit/Adafruit_nRF52_Bootloader.git

(cd Adafruit_nRF52_Bootloader && git am --keep-cr < ../0001-add-demo-badge-2023.patch)

(cd Adafruit_nRF52_Bootloader && git submodule update --init)

mkdir -p build/

set +o errexit

(cd Adafruit_nRF52_Bootloader && make BOARD=demo_badge_2023 all)
cp Adafruit_nRF52_Bootloader/_build/build-demo_badge_2023/demo_badge_2023_bootloader-*_nosd.hex build/bootloader.hex
cp Adafruit_nRF52_Bootloader/_build/build-demo_badge_2023/demo_badge_2023_bootloader-*.out build/bootloader.elf
cp Adafruit_nRF52_Bootloader/_build/build-demo_badge_2023/update-demo_badge_2023_bootloader-*.uf2 build/update-bootloader.uf2
