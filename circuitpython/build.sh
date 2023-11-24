#!/usr/bin/env bash

# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

set -o nounset
set -o pipefail
set -o xtrace
set -o errexit

cd "$(dirname ${BASH_SOURCE})"

rm -rf circuitpython
git clone --depth 1 --branch 8.2.7 https://github.com/adafruit/circuitpython.git

(cd circuitpython && git am ../0001-add-demo-badge-2023.patch)

(cd circuitpython/ports/nrf/ && make fetch-port-submodules)

(cd circuitpython && pip3 install -r requirements-dev.txt)
(cd circuitpython && make -C mpy-cross)

rm -rf circuitpython/_build/ builds/

mkdir -p builds/

(cd circuitpython/ports/nrf/ && make BOARD=demo_badge_2023)
cp circuitpython/ports/nrf/build-demo_badge_2023/firmware.uf2 builds/circuitpython.uf2
