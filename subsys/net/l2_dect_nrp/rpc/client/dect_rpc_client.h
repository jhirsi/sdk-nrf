/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_RPC_CLIENT_H_
#define DECT_RPC_CLIENT_H_

#include <stdint.h>
#include <stddef.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dect_mgmt.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Initialize DECT RPC client
 *
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_client_init(void);

/** @brief Activate DECT NR+ stack via RPC
 *
 * @param iface_index Network interface index (use 0 for default/first DECT interface)
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_activate(int iface_index);

/** @brief Deactivate DECT NR+ stack via RPC
 *
 * @param iface_index Network interface index (use 0 for default/first DECT interface)
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_deactivate(int iface_index);

/** @brief Get status information via RPC
 *
 * @param iface_index Network interface index (use 0 for default/first DECT interface)
 * @param status_info Output buffer for status information
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_status_info_get(int iface_index, struct dect_status_info *status_info);

/** @brief Read settings via RPC
 *
 * @param iface_index Network interface index (use 0 for default/first DECT interface)
 * @param settings Output buffer for settings
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_settings_read(int iface_index, struct dect_settings *settings);

/** @brief Write settings via RPC
 *
 * @param iface_index Network interface index (use 0 for default/first DECT interface)
 * @param settings Settings to write
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_settings_write(int iface_index, const struct dect_settings *settings);

/** @brief Subscribe to DECT events via RPC
 *
 * @param iface_index Network interface index (use 0 for default/first DECT interface)
 * @param event_mask Bitmask of events to subscribe to
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_event_subscribe(int iface_index, uint32_t event_mask);

/** @brief Activate DECT NR+ stack via RPC (using net_if - convenience function)
 *
 * @param iface Network interface (can be NULL to use default)
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_activate_iface(struct net_if *iface);

/** @brief Deactivate DECT NR+ stack via RPC (using net_if - convenience function)
 *
 * @param iface Network interface (can be NULL to use default)
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_deactivate_iface(struct net_if *iface);

/** @brief Register callback for DECT events received via RPC
 *
 * @param callback Callback function to call when events are received
 *                  iface_index will be -1 if interface is not available
 * @param user_data User data to pass to callback
 */
void dect_rpc_register_event_callback(
	void (*callback)(int iface_index, uint32_t event, const void *event_data, size_t event_data_len),
	void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* DECT_RPC_CLIENT_H_ */

