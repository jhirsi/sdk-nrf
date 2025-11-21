/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_RPC_MQTT_TRANSPORT_H_
#define DECT_RPC_MQTT_TRANSPORT_H_

#include <stdbool.h>
#include <net/nrf_cloud.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle nRF Cloud RX_DATA_GENERAL event
 *
 * This function should be called from the application's nRF Cloud event handler
 * when receiving NRF_CLOUD_EVT_RX_DATA_GENERAL events. It checks if the data
 * is a DECT RPC packet and routes it to the transport's receive handler.
 *
 * @param data Pointer to the received data from nRF Cloud
 *
 * @return true if the data was handled (DECT RPC packet), false otherwise
 */
bool dect_rpc_mqtt_transport_handle_rx_data(const struct nrf_cloud_data *data);

/**
 * @brief Get the MQTT transport instance
 *
 * @return Pointer to the nRF RPC transport structure for MQTT
 */
const struct nrf_rpc_tr *dect_rpc_mqtt_transport_get(void);

#ifdef __cplusplus
}
#endif

#endif /* DECT_RPC_MQTT_TRANSPORT_H_ */

