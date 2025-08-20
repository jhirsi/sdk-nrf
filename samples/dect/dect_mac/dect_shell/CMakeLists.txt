#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dect_shell_mdm_mac)
# To get the app name to cloud
zephyr_compile_definitions(PROJECT_NAME=${PROJECT_NAME})

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/shell.c)

add_subdirectory(src/print)
add_subdirectory(src/utils)
add_subdirectory_ifdef(CONFIG_DESH_NATIVE_TLS src/native_tls)
add_subdirectory_ifdef(CONFIG_DESH_L2_SHELL src/l2_shell)
add_subdirectory_ifdef(CONFIG_DESH_DECT_ICMP_PING src/ping)
add_subdirectory(src/at)
add_subdirectory(src/cloud)
add_subdirectory_ifdef(CONFIG_NET_HOSTNAME_DYNAMIC src/hostname)
add_subdirectory(src/auto_connect)

if (CONFIG_NRF_CLOUD_MQTT OR CONFIG_NRF_CLOUD_REST)
        # Make folder containing certificates global so that it can be located by the configured
        # cloud library.
        zephyr_include_directories(src/nrf_cloud_certs_mqtt)
endif()
if (CONFIG_NRF_CLOUD_COAP)
        # Make folder containing certificates global so that it can be located by the configured
        # cloud library.
        zephyr_include_directories(src/nrf_cloud_certs_coap)
endif()
