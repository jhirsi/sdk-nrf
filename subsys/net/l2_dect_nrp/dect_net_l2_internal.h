/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_NET_L2_INTERNAL_H_
#define DECT_NET_L2_INTERNAL_H_


#ifdef __cplusplus
extern "C" {
#endif

void dect_net_l2_status_info_fill_association_data(
	struct net_if *iface, struct dect_status_info *status_info_out);
void dect_net_l2_status_info_fill_sink_data(
	struct net_if *iface, struct dect_status_info *status_info_out);

void dect_net_l2_addr_util_global_addr_replace(struct net_if *dect_iface);


#ifdef __cplusplus
}
#endif

#endif /* DECT_NET_L2_INTERNAL_H_ */
