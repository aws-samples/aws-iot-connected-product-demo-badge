#!/usr/bin/env bash

set -o nounset
set -o pipefail
set -o xtrace
set -o errexit

export ROOT_DIR=$(dirname $BASH_SOURCE)
export BOOTLOADER_HEX="${ROOT_DIR}/adafruit_bootloader_demo_badge_2023/bootloader.hex"

. "${ROOT_DIR}/env.sh"

# generated files
export APP_HEX="${ROOT_DIR}/build/zephyr/zephyr.hex"
export APP_UF2="${ROOT_DIR}/build/zephyr/zephyr.uf2"
export MERGED_HEX="${ROOT_DIR}/build/merged.hex"

# write the (short) git commit ID and timestamp
if [ -z ${APP_GIT_SHA+x} ]; then
    export APP_GIT_SHA=$(git rev-parse --short HEAD)
    if [ -z ${APP_GIT_SHA} ]; then
        export APP_GIT_SHA="(no git information)"
    fi
fi
if [ -z ${BUILD_TIMESTAMP+x} ]; then
    export BUILD_TIMESTAMP=$(date +%Y-%m-%dT%H%M%S)
fi
envsubst < "${ROOT_DIR}/src/app_version.c.template" > "${ROOT_DIR}/src/app_version.c"

set +o xtrace

# build Zephyr firmware
west build --board demo_badge_2023 -- -DBOARD_ROOT=${ROOT_DIR}

# copy usable files to top-level build directory
cp "${APP_HEX}" "${ROOT_DIR}/build/zephyr.hex"
cp "${APP_UF2}" "${ROOT_DIR}/build/zephyr.uf2"

# This contains everything and should be flashed with JLink or SWD programmer:
# specify bootloader last, to take precendence in case of overlaps:
mergehex \
    --merge \
    ${APP_HEX} \
    ${BOOTLOADER_HEX} \
    --output ${MERGED_HEX}

if [[ $# -eq 1 && $1 == "-f" ]]; then
    # flash function must be defined in env.sh!
    flash "${MERGED_HEX}"
elif [[ $# -eq 1 && $1 == "-d" ]]; then
    # debug function must be defined in env.sh!
    debug "${ROOT_DIR}/build/zephyr/zephyr.elf"
fi
