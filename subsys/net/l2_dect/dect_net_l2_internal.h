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

struct dect_net_l2_association_data {
	bool in_use;
	uint32_t target_long_rd_id;

	bool local_ipv6_addr_set;
	struct in6_addr local_ipv6_addr;
	bool global_ipv6_addr_set;
	struct in6_addr global_ipv6_addr;
};

void dect_net_l2_status_info_fill_association_data(struct net_if *iface,
						   struct dect_status_info *status_info_out);
void dect_net_l2_status_info_fill_sink_data(struct net_if *iface,
					    struct dect_status_info *status_info_out);

void dect_net_l2_sink_ipv6_config_changed(struct net_if *iface,
					  struct dect_net_ipv6_prefix_config *ipv6_prefix_config);

/**
 * @brief Membership test against currently associated DECT child global IPv6
 *        addresses.
 *
 * Used by the bridge sink to recognise whether an Ethernet-side ICMPv6
 * Neighbor Solicitation is asking about an address that this device proxies
 * for on behalf of a PT. Returns false when no child currently advertises the
 * given address.
 *
 * Internal to the DECT L2 implementation; not part of the public API surface.
 *
 * @param addr Address to test (must be a global unicast address).
 * @return true if @p addr matches the global IPv6 address of any currently
 *         associated DECT child, false otherwise.
 */
bool dect_net_l2_child_global_ipv6_match(const struct in6_addr *addr);

#ifdef __cplusplus
}
#endif

#endif /* DECT_NET_L2_INTERNAL_H_ */
