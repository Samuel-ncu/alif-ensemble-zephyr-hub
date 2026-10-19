# Copyright (c) 2026 Samuel (Shu-Ming) Liu
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Configure Arm Ethos-U55 NPU (256 MACs on Cortex-M55 HP)
set(ETHOSU_TARGET_NPU_CONFIG "ethos-u55-256" CACHE STRING "Ethos-U NPU Configuration")
set(ETHOSU_ARCH "u55")
set(ETHOSU_MACS "256")
set(ETHOSU_ARCH_U55 y CACHE BOOL "U55 Flag")
add_compile_definitions(ETHOSU_ARCH_U55="y")
set(CONFIG_ARM_ETHOS_U55_256 y CACHE BOOL "U55 256 MACs" FORCE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(ethosu_npu_ai)

target_include_directories(app PRIVATE
    /home/user/alif/sdk-alif/alif/include
    /home/user/alif/sdk-alif/optional/modules/lib/tflite-micro
)

target_sources(app PRIVATE src/main.cpp)

