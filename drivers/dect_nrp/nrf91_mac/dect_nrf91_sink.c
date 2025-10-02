/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>

#include <dect_net_l2_mgmt.h>

#include <net/dect_nrp_utils.h>

#include "dect_nrf91_sink.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(DECT_NRP_MAC, CONFIG_DECT_NRP_MAC_LOG_LEVEL);

#include "net_private.h" /* For net_sprint_ipv6_addr */

struct dect_nrf91_sink_context {
	struct net_if *iface_for_dect;

	bool global_prefix_addr_set;
	struct in6_addr global_prefix_addr;
};

static struct dect_nrf91_sink_context dect_nrf91_sink_context_data;

static struct net_mgmt_event_callback dect_nrf91_net_mgmt_ipv6_event_cb;

bool dect_nrf91_sink_ipv6_prefix_get(struct dect_nrf91_ipv6_prefix *prefix_out)
{
	struct dect_nrf91_sink_context *ctx = &dect_nrf91_sink_context_data;

	if (ctx->global_prefix_addr_set == false || ctx->iface_for_dect == NULL) {
		return false;
	}
	prefix_out->len = sizeof(struct in6_addr) / 2;
	net_ipaddr_copy(&prefix_out->prefix, &ctx->global_prefix_addr);

	return true;
}

static void dect_nrf91_sink_net_mgmt_ipv6_event_handler(struct net_mgmt_event_callback *cb,
						   uint64_t mgmt_event, struct net_if *iface)
{
	char ipv6_addr_str[NET_IPV6_ADDR_LEN];
	struct dect_nrf91_sink_context *ctx = &dect_nrf91_sink_context_data;

	if (iface != ctx->iface_for_dect) {
		printk("Ignoring event for iface %p", iface);
		return;
	}

	if (mgmt_event == NET_EVENT_IPV6_PREFIX_ADD) {
		struct net_event_ipv6_prefix *ipv6_prefix =
			(struct net_event_ipv6_prefix *)cb->info;

		LOG_DBG("NET_EVENT_IPV6_PREFIX_ADD: iface %p, prefix %s/%d", iface,
			net_addr_ntop(AF_INET6, (struct in6_addr *)&ipv6_prefix->addr,
				      ipv6_addr_str, NET_IPV6_ADDR_LEN),
			ipv6_prefix->len);
		/* Save our used global prefix */
		if (!ctx->global_prefix_addr_set && ipv6_prefix->len == 64 &&
		    net_ipv6_is_global_addr((struct in6_addr *)&ipv6_prefix->addr)) {
			ctx->global_prefix_addr_set = true;
			memcpy(&ctx->global_prefix_addr, &ipv6_prefix->addr,
			       sizeof(ctx->global_prefix_addr));
			LOG_INF("NET_EVENT_IPV6_PREFIX_ADD: our global prefix set to %s/64",
				net_sprint_ipv6_addr(&ctx->global_prefix_addr));
		}

		/* TODO ilmota modemille jos muuttunut ja FT laite */

	} else if (mgmt_event == NET_EVENT_IPV6_PREFIX_DEL) {
		struct net_event_ipv6_prefix *ipv6_prefix =
			(struct net_event_ipv6_prefix *)cb->info;

		LOG_DBG("NET_EVENT_IPV6_PREFIX_DEL: iface %p, prefix %s/%d", iface,
			net_addr_ntop(AF_INET6, (struct in6_addr *)&ipv6_prefix->addr,
				      ipv6_addr_str, NET_IPV6_ADDR_LEN),
			ipv6_prefix->len);

		/* Check if this is the same as was selected as our used prefix */
		if (ctx->global_prefix_addr_set &&
		    net_ipv6_is_prefix(ctx->global_prefix_addr.s6_addr,
				       ipv6_prefix->addr.s6_addr,
				       64)) {
			ctx->global_prefix_addr_set = false;
			memset(&ctx->global_prefix_addr, 0, sizeof(ctx->global_prefix_addr));
			LOG_INF("NET_EVENT_IPV6_PREFIX_DEL: our global prefix was removed");
		}
		/* TODO ilmota modemille jos hävis jos FT laite */
	}
}

static int dect_nrf91_sink_init(void)
{
	struct dect_nrf91_sink_context *ctx = &dect_nrf91_sink_context_data;

	LOG_DBG("dect_nrf91_sink_init");

	memset(ctx, 0, sizeof(*ctx));

	ctx->iface_for_dect = net_if_get_by_index(
		net_if_get_by_name("nrf91_dect")); /* TODO: kconfig for the used name */

	if (!ctx->iface_for_dect) {
		LOG_ERR("%s: interface nrf91_dect not found", (__func__));
	}

	net_mgmt_init_event_callback(&dect_nrf91_net_mgmt_ipv6_event_cb,
				     dect_nrf91_sink_net_mgmt_ipv6_event_handler,
				     (NET_EVENT_IPV6_PREFIX_ADD | NET_EVENT_IPV6_PREFIX_DEL));
	net_mgmt_add_event_callback(&dect_nrf91_net_mgmt_ipv6_event_cb);

	return 0;
}

SYS_INIT(dect_nrf91_sink_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
