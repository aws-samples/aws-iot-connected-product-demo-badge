cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(demo-badge-2023)

FILE(GLOB app_sources
    src/*.c
    src/**/*.c
    ext/*.c
)

target_sources(app PRIVATE ${app_sources})

zephyr_include_directories(
    include
    ext
)
