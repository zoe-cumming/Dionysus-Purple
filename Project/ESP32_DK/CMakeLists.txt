# SPDX-License-Identifier: Apache-2.0

set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/socs/esp32c3_devkitm.overlay)

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_apsta_mode)

target_sources(app PRIVATE src/main.c)