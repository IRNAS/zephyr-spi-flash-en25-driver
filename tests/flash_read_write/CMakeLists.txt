# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})

project(LR1110_modem_lib)

zephyr_compile_options(-fdiagnostics-color=always)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

zephyr_include_directories(.)
zephyr_include_directories(src)

add_subdirectory(drivers)

FILE(GLOB app_sources src/*.c)

target_sources(app PRIVATE ${app_sources})
