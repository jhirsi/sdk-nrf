/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_RPC_SERIALIZE_H_
#define DECT_RPC_SERIALIZE_H_

#include <zephyr/net/net_if.h>
#include <zephyr/net/dect_mgmt.h>
#include <nrf_rpc/nrf_rpc_serialize.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Encode DECT settings structure
 *
 * @param ctx CBOR encoding context
 * @param settings Settings to encode
 */
void dect_rpc_encode_settings(struct nrf_rpc_cbor_ctx *ctx,
			       const struct dect_settings *settings);

/** @brief Decode DECT settings structure
 *
 * @param ctx CBOR decoding context
 * @param settings Output buffer for settings
 * @return true on success, false on failure
 */
bool dect_rpc_decode_settings(struct nrf_rpc_cbor_ctx *ctx,
			      struct dect_settings *settings);

/** @brief Encode DECT status info structure
 *
 * @param ctx CBOR encoding context
 * @param status_info Status info to encode
 */
void dect_rpc_encode_status_info(struct nrf_rpc_cbor_ctx *ctx,
				  const struct dect_status_info *status_info);

/** @brief Decode DECT status info structure
 *
 * @param ctx CBOR decoding context
 * @param status_info Output buffer for status info
 * @return true on success, false on failure
 */
bool dect_rpc_decode_status_info(struct nrf_rpc_cbor_ctx *ctx,
				 struct dect_status_info *status_info);

/** @brief Encode IPv6 address
 *
 * @param ctx CBOR encoding context
 * @param addr IPv6 address to encode
 */
void dect_rpc_encode_ipv6_addr(struct nrf_rpc_cbor_ctx *ctx,
				const struct in6_addr *addr);

/** @brief Decode IPv6 address
 *
 * @param ctx CBOR decoding context
 * @param addr Output buffer for IPv6 address
 * @return true on success, false on failure
 */
bool dect_rpc_decode_ipv6_addr(struct nrf_rpc_cbor_ctx *ctx,
				struct in6_addr *addr);

#ifdef __cplusplus
}
#endif

#endif /* DECT_RPC_SERIALIZE_H_ */

