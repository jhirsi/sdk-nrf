/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_TETHER_IPV6_INT_H__
#define DECT_TETHER_IPV6_INT_H__

#include <stdbool.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>

struct sockaddr_in6;

bool dect_tether_ipv6_dect_addrs_get(struct net_if *dect_iface, struct net_in6_addr *ula_out,
					 struct net_in6_addr *gua_out, bool *have_ula,
					 bool *have_gua);

int dect_tether_ipv6_ra_init(void);
void dect_tether_ipv6_ra_deinit(void);
#if defined(CONFIG_DECT_TETHER_IPV6_RA)
void dect_tether_ipv6_ra_kick(void);
#endif
/* Called by fwd.c when the DECT parent association is created / released so
 * that the RA module can guard its periodic RA against transient false returns
 * from dect_net_l2_parent_ipv6_addr_get().
 */
void dect_tether_ipv6_ra_set_uplink(bool present);

int dect_tether_ipv6_dhcpv6_srv_init(void);
void dect_tether_ipv6_dhcpv6_srv_deinit(void);
#if defined(CONFIG_DECT_TETHER_IPV6_DHCPV6_SERVER)
bool dect_tether_ipv6_dhcpv6_srv_lease_bound(void);
/** Bound lease GUA for unicast RA maintenance (false if no GUA on lease). */
bool dect_tether_ipv6_dhcpv6_srv_tether_gua(struct net_in6_addr *gua_out);
void dect_tether_ipv6_dhcpv6_srv_uplink_lost(void);
void dect_tether_ipv6_dhcpv6_srv_uplink_restored(void);
#if IS_ENABLED(CONFIG_DECT_TETHER_IPV6_RA_TETHER_MAINT) && \
	CONFIG_DECT_TETHER_IPV6_DHCPV6_LEASE_REFRESH_MIN_MS > 0
/** Proactive Reply for periodic maint; rate-limited by LEASE_REFRESH_MIN_MS. */
void dect_tether_ipv6_dhcpv6_srv_lease_refresh_request(void);
#endif
#endif

#if defined(CONFIG_DECT_TETHER_IPV6_RA_TETHER_MAINT)
/** Remember tether host link-local (and optional Ethernet MAC) for periodic unicast RAs. */
void dect_tether_ipv6_ra_tether_peer_note(const struct net_in6_addr *ll_addr,
					       const uint8_t *eth_mac, bool mac_valid);
#endif

#if defined(CONFIG_DECT_TETHER_IPV6_FWD)
int dect_tether_ipv6_fwd_init(void);
void dect_tether_ipv6_fwd_deinit(void);
void dect_tether_ipv6_fwd_tether_update(struct net_if *eth,
					      const struct sockaddr_in6 *cli,
					      const uint8_t *client_duid, uint16_t client_duid_len,
					      const struct net_in6_addr *ula,
					      const struct net_in6_addr *gua, bool have_ula,
					      bool have_gua);
#endif

#if defined(CONFIG_DECT_TETHER_IPV6_MDNS_FORWARD)
int dect_tether_ipv6_mdns_fwd_start(void);
void dect_tether_ipv6_mdns_fwd_stop(void);
#endif

#endif /* DECT_TETHER_IPV6_INT_H__ */
