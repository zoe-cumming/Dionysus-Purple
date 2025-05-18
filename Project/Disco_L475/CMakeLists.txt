# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Project)

# Collect all the source files in app/prac2/src
#FILE(GLOB Prac3_sources)

# Collect all the source files in lib. Note that
# the path given needs to be relative to the
# CMakeLists root, which is app/prac2 here,
# hence the ../../lib.c.
#FILE(GLOB lib_sources ../lib/*.c)

# Tell CMake to build with the app and lib sources
target_sources(app PRIVATE src/main.c)

# Tell CMake where our header files are
#target_include_directories(app PRIVATE ../include)