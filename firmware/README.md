# Demo Badge Firmeware

## Setting up a development environment

1. Install nRF Connect SDK 2.5.0: https://developer.nordicsemi.com/nRF_Connect_SDK/doc/2.5.0/nrf/installation.html
2. Install ncs-sidewalk 2.5.0 matching to nRF Connect SDK: https://nrfconnect.github.io/sdk-sidewalk/setting_up_sidewalk_environment/setting_up_sdk.html
3. Update `./env.sh` to include all environment variables and PATH to the SDKs

## Compile firmware

1. Run `./build.sh`

Use `build/zephyr.uf2` for UF2-based flashing over USB mass storage bootloader.

Use `build/merged.hex` for OpenOCD-based flashing using hardware programmer.
