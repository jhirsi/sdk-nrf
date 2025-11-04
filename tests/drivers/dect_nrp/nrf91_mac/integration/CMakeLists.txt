# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2025 Nordic Semiconductor ASA
# DECT NR+ Stack Integration Tests with Unity and CMock

cmake_minimum_required(VERSION 3.20.0)

# Use our custom Kconfig for test-specific configuration
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dect_nrp_integration_test)

# Unity Test Framework is provided by CONFIG_UNITY=y in prj.conf and Nordic SDK
# CMock headers are included but implementations come from Nordic SDK

# Add test source files for Unity-based integration testing
# Test source files
target_sources(app PRIVATE
    src/main.c
    src/test_dect_integration.c
    src/test_dect_utils.c
    src/mocks/mock_nrf_modem_dect_mac.c
)

# The DECT L2 subsystem is now included automatically via Zephyr modules
# when CONFIG_NET_L2_DECT_MGMT=y is enabled in prj.conf
# No need to manually add the source files

# Include all necessary directories for DECT stack
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mocks
    ${ZEPHYR_BASE}/../nrfxlib/nrf_modem/include
    ${ZEPHYR_BASE}/../nrfxlib/common/include
    ${ZEPHYR_BASE}/../nrf/drivers/dect_nrp/nrf91_mac
    ${ZEPHYR_BASE}/../nrf/subsys/net/l2_dect_nrp/include
    ${ZEPHYR_BASE}/../nrf/include/net
    ${ZEPHYR_BASE}/include/zephyr/net
    ${ZEPHYR_BASE}/subsys/net/ip
)

# Add nrfxlib include directories globally so DECT stack can find headers
target_include_directories(zephyr_interface INTERFACE
    ${ZEPHYR_BASE}/../nrfxlib/nrf_modem/include
    ${ZEPHYR_BASE}/../nrfxlib/common/include
)

# All CONFIG values are now properly defined in Kconfig
# No need for manual compile definitions

# Unity and CMock libraries are provided by Nordic SDK modules

# Add custom target for coverage report generation
if(CONFIG_COVERAGE)
    add_custom_target(coverage_report
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/generate_coverage.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating DECT NR+ stack coverage report"
        DEPENDS app
    )
endif()