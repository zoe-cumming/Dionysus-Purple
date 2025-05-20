# SPDX-License-Identifier: Apache-2.0

set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/esp32s3_eye_procpu.overlay)

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(esp32_camnode)

file(GLOB app_sources src/*.c)
zephyr_library_sources(${app_sources})