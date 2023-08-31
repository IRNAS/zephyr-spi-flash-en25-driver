# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# create compile_commands.json for clang
set(CMAKE_EXPORT_COMPILE_COMMANDS on)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sample_test)

# Make a helper variable for the app directory
set(APP_DIR ../../app/main/src)

# Add source files with test code
file(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
