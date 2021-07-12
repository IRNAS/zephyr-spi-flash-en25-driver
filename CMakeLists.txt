cmake_minimum_required(VERSION 3.13.1)

zephyr_compile_options(-fdiagnostics-color=always)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(drivers)
