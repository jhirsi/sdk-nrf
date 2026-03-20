#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_DECT_NR_RPC_NET_IF dect_rpc_if.c)
zephyr_library_sources_ifdef(CONFIG_DECT_NR_RPC_CLIENT_SHELL dect_rpc_shell.c)

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
)
# So app (e.g. dect_rpc_client sample) can #include "dect_rpc_client_net.h"
target_include_directories(${ZEPHYR_CURRENT_LIBRARY} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Ensure app links this library so the DECT RPC net_if is registered (avoids
# being dropped when nothing in app directly references client objects).
if(TARGET app)
  target_link_libraries(app PRIVATE ${ZEPHYR_CURRENT_LIBRARY})
endif()
