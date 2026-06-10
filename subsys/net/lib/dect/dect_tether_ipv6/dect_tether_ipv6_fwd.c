/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/sys/byteorder.h>

#include <net/dect/dect_net_l2.h>
#include <net/dect/dect_net_l2_mgmt.h>

LOG_MODULE_REGISTER(dect_tether_ipv6_fwd, CONFIG_DECT_TETHER_IPV6_LOG_LEVEL);

#include "dect_tether_ipv6_int.h"

#include "ipv6.h"
#include "nbr.h"
#include "net_private.h"
#include "route.h"

#define DUID_TYPE_LLT 1
#define DUID_TYPE_LL  3
#define HW_TYPE_ETH   1

static struct net_route_entry *rt_ula;
static struct net_route_entry *rt_gua;
static struct net_if_router *dect_def_router;
static struct net_mgmt_event_callback mgmt_cb;
static struct k_mutex tether_mu;

static struct net_if *iface_dect(void)
{
	return net_if_get_first_by_type(&NET_L2_GET_NAME(DECT));
}

static void tether_routes_remove_locked(void)
{
	if (rt_ula != NULL) {
		(void)net_route_del(rt_ula);
		rt_ula = NULL;
	}
	if (rt_gua != NULL) {
		(void)net_route_del(rt_gua);
		rt_gua = NULL;
	}
}

static void dect_parent_router_clear(void)
{
	if (dect_def_router != NULL) {
		(void)net_if_ipv6_router_rm(dect_def_router);
		dect_def_router = NULL;
	}
}

static void dect_parent_router_refresh(struct net_if *dect)
{
	struct net_in6_addr parent;

	dect_parent_router_clear();

	if (dect == NULL) {
		return;
	}
	if (!dect_net_l2_parent_ipv6_addr_get(&parent)) {
		return;
	}

	dect_def_router = net_if_ipv6_router_add(dect, &parent, true, 65535U);
	if (dect_def_router == NULL) {
		LOG_WRN("fwd: net_if_ipv6_router_add failed for %s",
			net_sprint_ipv6_addr((const struct in6_addr *)&parent));
	} else {
		LOG_INF("fwd: default IPv6 router on DECT via parent %s",
			net_sprint_ipv6_addr((const struct in6_addr *)&parent));
	}
}

static bool duid_extract_eth_mac(const uint8_t *duid, uint16_t dlen, uint8_t mac[6])
{
	if (duid == NULL) {
		return false;
	}
	/* DUID-LL: type 3, 16-bit hardware type, link-layer address */
	if (dlen >= 10U && sys_get_be16(duid) == DUID_TYPE_LL &&
	    sys_get_be16(duid + 2) == HW_TYPE_ETH) {
		memcpy(mac, duid + 4, 6U);
		return true;
	}
	/* DUID-LLT: type 1, 16-bit hardware type, 32-bit time, link-layer address */
	if (dlen >= 14U && sys_get_be16(duid) == DUID_TYPE_LLT &&
	    sys_get_be16(duid + 2) == HW_TYPE_ETH) {
		memcpy(mac, duid + 8, 6U);
		return true;
	}
	return false;
}

static bool eth_peer_ll_lookup(struct net_if *eth, const struct sockaddr_in6 *cli,
			       const uint8_t *duid, uint16_t dlen, struct net_linkaddr *lla)
{
	struct net_nbr *nbr;
	const struct net_linkaddr *ll;

	if (eth == NULL || cli == NULL || lla == NULL) {
		return false;
	}

	if (net_ipv6_is_ll_addr((struct net_in6_addr *)&cli->sin6_addr)) {
		nbr = net_ipv6_nbr_lookup(eth, (struct net_in6_addr *)&cli->sin6_addr);
		if (nbr != NULL && nbr->idx != NET_NBR_LLADDR_UNKNOWN) {
			ll = net_nbr_get_lladdr(nbr->idx);
			if (ll != NULL && ll->len == 6U) {
				lla->len = 6U;
				lla->type = NET_LINK_ETHERNET;
				memcpy(lla->addr, ll->addr, 6U);
				return true;
			}
		}
	}

	if (duid_extract_eth_mac(duid, dlen, lla->addr)) {
		lla->len = 6U;
		lla->type = NET_LINK_ETHERNET;
		return true;
	}
	return false;
}

static int tether_one_addr(struct net_if *eth, const struct net_in6_addr *addr,
			   const struct net_linkaddr *lla)
{
	struct net_nbr *nbr;

	if (addr == NULL || lla == NULL) {
		return -EINVAL;
	}

	/* Pin as STATIC so the entry — and the /128 route that relies on it as
	 * nexthop — never decays while the PC is tethered.  Without this the
	 * entry follows REACHABLE→STALE→PROBE→fail→delete once traffic quiets
	 * for ~30 s (e.g. during a NUD cycle on the host side).  Zephyr's
	 * net_ipv6_nbr_rm() internally calls net_route_del_by_nexthop(), so
	 * deleting the neighbor also removes the /128 route.  Inbound packets
	 * then fall through to the DECT default route instead of reaching the
	 * PC on Ethernet, causing persistent connectivity loss until the next
	 * DHCPv6 Renew re-installs the route.
	 *
	 * Remove first: net_ipv6_nbr_add() preserves the existing state on
	 * update, so a fresh STATIC entry requires an explicit rm beforehand.
	 */
	(void)net_ipv6_nbr_rm(eth, (struct net_in6_addr *)addr);
	nbr = net_ipv6_nbr_add(eth, addr, lla, false, NET_IPV6_NBR_STATE_STATIC);
	if (nbr == NULL) {
		LOG_WRN("fwd: net_ipv6_nbr_add(STATIC) failed for %s",
			net_sprint_ipv6_addr((const struct in6_addr *)addr));
		return -EIO;
	}

	return 0;
}

static int tether_route_install(struct net_if *eth, const struct net_in6_addr *addr,
				struct net_route_entry **slot)
{
	struct net_route_entry *re;

	if (eth == NULL || addr == NULL || slot == NULL) {
		return -EINVAL;
	}

	if (*slot != NULL) {
		(void)net_route_del(*slot);
		*slot = NULL;
	}

	re = net_route_add(eth, (struct net_in6_addr *)addr, 128,
			   (struct net_in6_addr *)addr, NET_IPV6_ND_INFINITE_LIFETIME,
			   NET_ROUTE_PREFERENCE_HIGH);
	if (re == NULL) {
		LOG_WRN("fwd: net_route_add /128 failed for %s",
			net_sprint_ipv6_addr((const struct in6_addr *)addr));
		return -EIO;
	}

	*slot = re;
	LOG_INF("fwd: /128 route on Ethernet for %s",
		net_sprint_ipv6_addr((const struct in6_addr *)addr));
	return 0;
}

void dect_tether_ipv6_fwd_tether_update(struct net_if *eth,
					     const struct sockaddr_in6 *cli,
					     const uint8_t *client_duid, uint16_t client_duid_len,
					     const struct net_in6_addr *ula,
					     const struct net_in6_addr *gua, bool have_ula,
					     bool have_gua)
{
	struct net_linkaddr lla = { 0 };

	if (!IS_ENABLED(CONFIG_DECT_TETHER_IPV6_FWD)) {
		return;
	}
	if (eth == NULL || cli == NULL || (!have_ula && !have_gua)) {
		return;
	}

	k_mutex_lock(&tether_mu, K_FOREVER);
	tether_routes_remove_locked();

	if (!eth_peer_ll_lookup(eth, cli, client_duid, client_duid_len, &lla)) {
		LOG_WRN("fwd: no Ethernet link-layer for tether (DUID-LL/LLT or neighbor on fe80)");
		k_mutex_unlock(&tether_mu);
		return;
	}

	/* Pin the PC's link-local neighbor as STATIC so it never decays.
	 * The LL entry is only ever used as the DHCPv6 unicast destination;
	 * without this it decays REACHABLE->STALE->PROBE->FAIL between Renew
	 * cycles (T1=120 s) because no upper-layer data traffic uses the LL
	 * address to trigger reachability confirmation.  A STATIC entry is
	 * exempt from NUD (ipv6_nbr_set_state is a no-op once STATIC) so the
	 * entry persists until the tether gateway reboots.
	 *
	 * net_ipv6_nbr_add() only applies the requested state to brand-new
	 * entries; add_nbr() returns any existing entry unchanged.  We must
	 * therefore remove the existing entry first so that the add below
	 * creates a fresh STATIC entry.  net_ipv6_nbr_rm() runs
	 * net_route_del_by_nexthop() internally, but the PC's LL address is
	 * never a route nexthop, so this is a safe no-op for routes.
	 */
	(void)net_ipv6_nbr_rm(eth, (struct net_in6_addr *)&cli->sin6_addr);
	{
		struct net_nbr *ll_nbr;

		ll_nbr = net_ipv6_nbr_add(eth,
					  (const struct net_in6_addr *)&cli->sin6_addr,
					  &lla, false,
					  NET_IPV6_NBR_STATE_STATIC);
		if (ll_nbr == NULL) {
			LOG_WRN("fwd: net_ipv6_nbr_add(STATIC) failed for PC LL %s",
				net_sprint_ipv6_addr(&cli->sin6_addr));
		}
	}

	if (have_ula) {
		(void)tether_one_addr(eth, ula, &lla);
		(void)tether_route_install(eth, ula, &rt_ula);
	}
	if (have_gua) {
		(void)tether_one_addr(eth, gua, &lla);
		(void)tether_route_install(eth, gua, &rt_gua);
	}

	k_mutex_unlock(&tether_mu);
}

static void fwd_mgmt_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			     struct net_if *iface)
{
	struct net_if *dect = iface_dect();

	if (mgmt_event != NET_EVENT_DECT_ASSOCIATION_CHANGED || iface != dect || iface == NULL) {
		return;
	}

#if !IS_ENABLED(CONFIG_NET_MGMT_EVENT_INFO)
	ARG_UNUSED(cb);
	return;
#else
	{
		const struct dect_association_changed_evt *evt;

		if (cb->info == NULL ||
		    cb->info_length < sizeof(struct dect_association_changed_evt)) {
			return;
		}
		evt = (const struct dect_association_changed_evt *)cb->info;

		if (evt->neighbor_role == DECT_NEIGHBOR_ROLE_PARENT &&
		    evt->association_change_type == DECT_ASSOCIATION_CREATED) {
			dect_parent_router_refresh(dect);
#if defined(CONFIG_DECT_TETHER_IPV6_DHCPV6_SERVER)
			dect_tether_ipv6_dhcpv6_srv_uplink_restored();
#endif
			dect_tether_ipv6_ra_set_uplink(true);
#if defined(CONFIG_DECT_TETHER_IPV6_RA)
			dect_tether_ipv6_ra_kick();
#endif
		} else if (evt->neighbor_role == DECT_NEIGHBOR_ROLE_PARENT &&
			   evt->association_change_type == DECT_ASSOCIATION_RELEASED) {
#if defined(CONFIG_DECT_TETHER_IPV6_DHCPV6_SERVER)
			dect_tether_ipv6_dhcpv6_srv_uplink_lost();
#endif
			dect_tether_ipv6_ra_set_uplink(false);
			dect_parent_router_clear();
			k_mutex_lock(&tether_mu, K_FOREVER);
			tether_routes_remove_locked();
			k_mutex_unlock(&tether_mu);
#if defined(CONFIG_DECT_TETHER_IPV6_RA)
			dect_tether_ipv6_ra_kick();
#endif
		}
	}
#endif
}

int dect_tether_ipv6_fwd_init(void)
{
	struct net_if *dect;

	if (!IS_ENABLED(CONFIG_DECT_TETHER_IPV6_FWD)) {
		return 0;
	}

	k_mutex_init(&tether_mu);
	dect = iface_dect();
	dect_parent_router_refresh(dect);

	net_mgmt_init_event_callback(&mgmt_cb, fwd_mgmt_handler,
				    NET_EVENT_DECT_ASSOCIATION_CHANGED);
	net_mgmt_add_event_callback(&mgmt_cb);

	LOG_INF("fwd: DECT parent default router + DHCP-driven /128 + nbr on Ethernet");
	return 0;
}

void dect_tether_ipv6_fwd_deinit(void)
{
	if (!IS_ENABLED(CONFIG_DECT_TETHER_IPV6_FWD)) {
		return;
	}

	net_mgmt_del_event_callback(&mgmt_cb);
	dect_parent_router_clear();
	k_mutex_lock(&tether_mu, K_FOREVER);
	tether_routes_remove_locked();
	k_mutex_unlock(&tether_mu);
}
