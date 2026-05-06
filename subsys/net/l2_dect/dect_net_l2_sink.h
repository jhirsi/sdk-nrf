/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_NET_L2_SINK_H
#define DECT_NET_L2_SINK_H

#include <zephyr/net/net_ip.h>

struct net_if;

/* Define for the prefix length that we use from BR iface global prefix */
#define DECT_NET_L2_SINK_IPV6_PREFIX_LEN_BYTES 8

struct dect_net_l2_sink_ipv6_prefix {

	/** IPv6 prefix */
	struct in6_addr prefix;

	/** Backpointer to network interface where this prefix is used */
	struct net_if *iface;

	/** Prefix length in bytes */
	uint8_t len;
};

#if defined(CONFIG_NET_L2_DECT_BR)
bool dect_net_l2_sink_ipv6_prefix_get(struct dect_net_l2_sink_ipv6_prefix *prefix_out);

/** Rebuild learned sink prefix from uplink /64 +
 *  current L2 transmitter_long_rd_id (e.g. after settings).
 */
void dect_net_l2_sink_reapply_prefix_for_tx_rd(struct net_if *dect_iface);
#else
static inline bool dect_net_l2_sink_ipv6_prefix_get(struct dect_net_l2_sink_ipv6_prefix *prefix_out)
{
	return false;
}

static inline void dect_net_l2_sink_reapply_prefix_for_tx_rd(struct net_if *dect_iface)
{
	(void)dect_iface;
}
#endif /* CONFIG_NET_L2_DECT_BR */

#if defined(CONFIG_NET_L2_DECT_BR_UNSOLICITED_NA) && \
	defined(CONFIG_NET_L2_DECT_BR_IPV6_ETH_ND_PROXY_PT)
/** Unsolicited NA on Ethernet sink for PT GUA (ND proxy);
 *  informs LAN peers of MAC mapping.
 */
void dect_net_l2_sink_eth_unsol_na_pt_nd_proxy(const struct in6_addr *tgt);
#endif

#if defined(CONFIG_NET_L2_DECT_BR_IPV6_ETH_ND_PROXY_PT_NS_PRIME)
/** Send Neighbor Solicitation on Ethernet with source = PT GUA and
 *  SLLAO = sink Ethernet MAC. Causes the upstream router (and any other
 *  on-link receiver per RFC 4861 7.2.3) to create or refresh a Neighbor
 *  Cache entry for the PT GUA even when none existed before, unlike an
 *  unsolicited NA which only updates pre-existing entries.
 *
 *  @p ctx is a short tag (e.g. "initial", "periodic") embedded in the
 *  INF-level log line so the trigger is visible in shell logs.
 */
void dect_net_l2_sink_eth_pt_nd_proxy_ns_prime(const struct in6_addr *pt_global,
					    const char *ctx);
#endif

#if defined(CONFIG_NET_L2_DECT_BR_IPV6_ETH_ND_PROXY_PT_NA_UNICAST_REFRESH)
/** Unicast NA on Ethernet addressed to the default router, for PT GUA.
 *  No-op when no default router is known. Pairs with the multicast
 *  unsolicited NA from dect_net_l2_sink_eth_unsol_na_pt_nd_proxy().
 *
 *  @p ctx is a short tag (e.g. "initial", "periodic") embedded in the
 *  INF-level log line so the trigger is visible in shell logs.
 */
void dect_net_l2_sink_eth_pt_nd_proxy_na_unicast(const struct in6_addr *pt_global,
					      const char *ctx);
#endif

#endif /* DECT_NET_L2_SINK_H */
