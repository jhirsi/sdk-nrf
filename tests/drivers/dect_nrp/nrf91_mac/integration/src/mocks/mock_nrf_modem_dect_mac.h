/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * CMock control functions for nrf_modem_dect_mac.h
 * Uses real API definitions from nrfxlib, only provides test control functions
 */

#ifndef MOCK_NRF_MODEM_DECT_MAC_H_
#define MOCK_NRF_MODEM_DECT_MAC_H_

#include <stdint.h>
#include <stdbool.h>
/* Note: Real API definitions come from nrf_modem_dect_mac.h included in source files */

#ifdef __cplusplus
extern "C" {
#endif
/* Include real API for structures, but we'll mock the implementations */
#include <nrf_modem_dect_mac.h>

/* Mock call tracking variables */
extern int mock_nrf_modem_dect_mac_callback_set_call_count;
extern int mock_nrf_modem_dect_mac_network_scan_call_count;
extern int mock_nrf_modem_dect_mac_cluster_beacon_receive_call_count;
extern int mock_nrf_modem_dect_mac_association_call_count;
extern int mock_nrf_modem_dect_mac_association_release_call_count;
extern int mock_nrf_modem_dect_mac_neighbor_list_call_count;
extern int mock_nrf_modem_dect_mac_neighbor_info_call_count;
extern int mock_nrf_modem_dect_control_systemmode_set_call_count;
extern int mock_nrf_modem_dect_control_configure_call_count;
extern int mock_nrf_modem_dect_control_functional_mode_set_call_count;
extern int mock_nrf_modem_dect_mac_rssi_scan_call_count;
extern int mock_nrf_modem_dect_mac_rssi_scan_stop_call_count;
extern int mock_nrf_modem_dect_mac_cluster_configure_call_count;
extern bool mock_cluster_creation_band1; /* Flag to indicate cluster creation RSSI scan at band 1 */

/* Mock return values */
extern int mock_nrf_modem_dect_mac_callback_set_return;
extern int mock_nrf_modem_dect_mac_network_scan_return;
extern int mock_nrf_modem_dect_mac_cluster_beacon_receive_return;

/* Stored callbacks for simulation */
extern struct nrf_modem_dect_mac_ntf_callbacks mock_ntf_callbacks;
extern struct nrf_modem_dect_mac_op_callbacks mock_op_callbacks;

/* CMock control functions for tests - do NOT redefine anything from real API */

/**
 * Reset the mock to initial state
 */
void mock_nrf_modem_dect_mac_reset(void);

/**
 * CMock-style expectation functions
 */
void mock_nrf_modem_dect_mac_callback_set_ExpectAndReturn(int return_value);
void mock_nrf_modem_dect_mac_network_scan_ExpectAndReturn(int return_value);
void mock_nrf_modem_dect_mac_cluster_beacon_receive_ExpectAndReturn(int return_value);
void mock_nrf_modem_dect_mac_network_scan_ExpectAndReturn(int return_value);
void mock_nrf_modem_dect_mac_association_ExpectAndReturn(int return_value);
int mock_nrf_modem_dect_control_systemmode_set_ExpectAndReturn(int return_value);
int mock_nrf_modem_dect_mac_configure_ExpectAndReturn(int return_value);

/**
 * Set return value for next function call
 * @param return_value The value to return from next mock function call
 */
void mock_nrf_modem_dect_mac_set_return_value(int return_value);

/**
 * Get number of mock function calls made
 * @return Number of calls to mock functions
 */
int mock_nrf_modem_dect_mac_get_call_count(void);

/**
 * Check if mock is initialized
 * @return True if initialized, false otherwise
 */
bool mock_nrf_modem_dect_mac_is_initialized(void);

/**
 * Simulate cluster beacon reception
 */
void mock_simulate_cluster_beacon_received(void);

/**
 * Simulate libmodem initialization (NRF_MODEM_LIB_ON_INIT callback)
 */
void mock_simulate_nrf_modem_lib_init(void);

/**
 * Expose callbacks for test access
 */
extern struct nrf_modem_dect_mac_ntf_callbacks mock_ntf_callbacks;
extern struct nrf_modem_dect_mac_op_callbacks mock_op_callbacks;

#ifdef __cplusplus
}
#endif

#endif /* MOCK_NRF_MODEM_DECT_MAC_H_ */
