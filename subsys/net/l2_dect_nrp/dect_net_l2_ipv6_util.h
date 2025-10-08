/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_NET_L2_IPV6_UTIL_H_
#define DECT_NET_L2_IPV6_UTIL_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "dect_net_l2_internal.h"

void dect_net_l2_util_parent_added_ipv6_addressing_handle(
	struct dect_net_l2_association_data *list_item,
	struct net_if *iface, uint32_t parent_long_rd_id,
	struct dect_net_ipv6_prefix_config *ipv6_prefix_config);
void dect_net_l2_util_child_added_ipv6_addressing_handle(
	struct dect_net_l2_association_data *ass_list_item, struct net_if *iface,
	uint32_t child_long_rd_id, bool first_child);

void dect_net_l2_util_child_removed_ipv6_addressing_handle(
	struct dect_net_l2_association_data *ass_list_item,
	struct net_if *iface, uint32_t child_long_rd_id);
void dect_net_l2_util_child_global_addr_removed_ipv6_addressing_handle(
	struct dect_net_l2_association_data *ass_list_item, struct net_if *iface);
void dect_net_l2_util_child_global_addr_changed_ipv6_addressing_handle(
	struct dect_net_l2_context *ctx,
	struct dect_net_l2_association_data *ass_list_item,
	struct net_if *iface);

void dect_net_l2_util_parent_removed_ipv6_addressing_handle(
	struct dect_net_l2_association_data *ass_list_item,
	struct net_if *iface, uint32_t parent_long_rd_id);

void dect_net_l2_addr_util_global_addr_replace(struct net_if *dect_iface);

void dect_net_l2_util_parent_ipv6_addressing_changed_handle(
	struct dect_net_l2_association_data *list_item,
	struct net_if *iface, uint32_t parent_long_rd_id,
	struct dect_net_ipv6_prefix_config *ipv6_prefix_config);

bool dect_net_l2_util_sink_ipv6_addressing_changed_handle(
	struct net_if *iface, struct dect_net_ipv6_prefix_config *ipv6_prefix_config);

#ifdef __cplusplus
}
#endif

#endif /* DECT_NET_L2_IPV6_UTIL_H_ */
