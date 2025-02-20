# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_fundamentals)

# STEP x - Generate HEX files from PEM files
set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/certs)
zephyr_include_directories(${gen_dir})
generate_inc_file_for_target(
   app
   certificate/AmazonRootCA1.pem
   ${gen_dir}/AmazonRootCA1.pem.inc
    )

target_sources(app PRIVATE src/main.c)