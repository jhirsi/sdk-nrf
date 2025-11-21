/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrf_rpc_cbor.h>

/* Check for custom DECT RPC transports first (MQTT) before base transports */
#if IS_ENABLED(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
#include "dect_rpc_mqtt_transport.h"
#elif IS_ENABLED(CONFIG_NRF_RPC_IPC_SERVICE) && !IS_ENABLED(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
#include <zephyr/device.h>
#include <nrf_rpc/nrf_rpc_ipc.h>
#elif IS_ENABLED(CONFIG_NRF_RPC_UART_TRANSPORT) && !IS_ENABLED(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
#include <nrf_rpc/nrf_rpc_uart.h>
#elif IS_ENABLED(CONFIG_MOCK_NRF_RPC_TRANSPORT)
#include <mock_nrf_rpc_transport.h>
#endif

/* Check for custom DECT RPC transports first (MQTT) before base transports */
#if IS_ENABLED(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
/* MQTT transport - use extern reference to the transport structure */
/* The transport structure is now non-static in dect_rpc_mqtt_transport.c */
extern const struct nrf_rpc_tr dect_rpc_mqtt_transport;
#define dect_rpc_tr dect_rpc_mqtt_transport
#elif IS_ENABLED(CONFIG_NRF_RPC_IPC_SERVICE) && !IS_ENABLED(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
NRF_RPC_IPC_TRANSPORT(dect_rpc_tr, DEVICE_DT_GET(DT_NODELABEL(ipc0)), "dect_rpc_ept");
#elif IS_ENABLED(CONFIG_NRF_RPC_UART_TRANSPORT) && !IS_ENABLED(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
#define dect_rpc_tr NRF_RPC_UART_TRANSPORT(DT_CHOSEN(nordic_rpc_uart))
#elif IS_ENABLED(CONFIG_MOCK_NRF_RPC_TRANSPORT)
#define dect_rpc_tr mock_nrf_rpc_tr
#else
#error "No RPC transport selected for DECT RPC"
#endif

/* For MQTT transport, use NOWAIT to avoid blocking on initialization
 * since there's no direct peer connection - clients connect via cloud.
 * NOWAIT means nrf_rpc_init() won't wait for group binding.
 */
#if IS_ENABLED(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
NRF_RPC_GROUP_DEFINE_NOWAIT(dect_rpc_group, "dect_rpc", &dect_rpc_tr,
			    NULL, /* ack_handler */
			    NULL, /* ack_data */
			    NULL, /* err_handler */
			    NULL, /* bound_handler */
			    false /* initiator - false means we're a follower, client initiates */);
#else
NRF_RPC_GROUP_DEFINE(dect_rpc_group, "dect_rpc", &dect_rpc_tr, NULL, NULL, NULL);
#endif

