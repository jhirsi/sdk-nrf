

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <dect_net_l2_mgmt.h>

#if defined(CONFIG_MODEM_CELLULAR)
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#endif

#if defined(CONFIG_NET_CONNECTION_MANAGER)
#include <zephyr/net/conn_mgr_monitor.h>
#endif

#include "route.h"
#include "ipv6.h"

#include <net/dect_nrp_utils.h>

#include "dect_net_l2_internal.h"
#include "dect_net_l2_ipv6_util.h"
#include "dect_net_l2_sink.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(NET_L2_DECT_BR, CONFIG_NET_L2_DECT_BR_LOG_LEVEL);

#include "net_private.h" /* For net_sprint_ipv6_addr */

static struct net_if *iface_for_prefix;
static struct net_if *iface_for_dect;

struct in6_addr sink_prefix_addr;
static bool sink_prefix_addr_set;

static struct in6_addr ipv6_router_addr;

#if defined(CONFIG_MODEM_CELLULAR)
const struct device *modem = DEVICE_DT_GET(DT_ALIAS(modem));

static struct k_work_delayable lte_ipv6_router_nbr_deleted_work;

#endif

/* TODO: this module to be moved to L2 as dect_net_l2_sink.c */

/**************************************************************************************************/

static struct net_mgmt_event_callback dect_net_l2_net_mgmt_ipv6_event_cb;

static void dect_net_l2_net_mgmt_ipv6_event_handler(struct net_mgmt_event_callback *cb,
						   uint64_t mgmt_event, struct net_if *iface)
{
	char ipv6_addr_str[NET_IPV6_ADDR_LEN];

	if (iface && iface != iface_for_prefix) {
		/* With NULL interface we should continue */
		return;
	}

	switch (mgmt_event) {
	case NET_EVENT_IPV6_PREFIX_ADD: {
		struct net_event_ipv6_prefix *ipv6_prefix =
			(struct net_event_ipv6_prefix *)cb->info;

		LOG_DBG("NET_EVENT_IPV6_PREFIX_ADD: iface %p, prefix %s/%d", iface,
			net_addr_ntop(AF_INET6, (struct in6_addr *)&ipv6_prefix->addr,
				      ipv6_addr_str, NET_IPV6_ADDR_LEN),
			ipv6_prefix->len);
		break;
	}
	case NET_EVENT_IPV6_PREFIX_DEL: {
		struct net_event_ipv6_prefix *ipv6_prefix =
			(struct net_event_ipv6_prefix *)cb->info;

		LOG_DBG("NET_EVENT_IPV6_PREFIX_DEL: iface %p, prefix %s/%d", iface,
			net_addr_ntop(AF_INET6, (struct in6_addr *)&ipv6_prefix->addr,
				      ipv6_addr_str, NET_IPV6_ADDR_LEN),
			ipv6_prefix->len);
		break;
	}
	case NET_EVENT_IPV6_ROUTER_DEL: {
		struct in6_addr *router_addr = (struct in6_addr *)cb->info;

		LOG_DBG("NET_EVENT_IPV6_ROUTER_DEL: iface %p, router %s", iface,
			net_addr_ntop(AF_INET6, router_addr, ipv6_addr_str, NET_IPV6_ADDR_LEN));
		if (net_if_is_up(iface_for_prefix) &&
		    net_ipv6_addr_cmp(&ipv6_router_addr, router_addr)) {
			struct dect_sink_status_evt sink_status_data = {
				.sink_status = DECT_SINK_STATUS_DISCONNECTED,
				.br_iface = iface_for_prefix,
			};

			LOG_INF("SINK: Router with IPv6 addr %s deleted from sink iface %p",
				net_addr_ntop(AF_INET6, router_addr, ipv6_addr_str,
					      NET_IPV6_ADDR_LEN),
				iface_for_prefix);

			sink_prefix_addr_set = false;
			dect_mgmt_sink_status_evt(iface_for_dect, sink_status_data);

			LOG_INF("SINK: starting router solicitation for iface %p",
				iface_for_prefix);
			net_if_start_rs(iface_for_prefix);
		}
		break;
	}
	case NET_EVENT_IPV6_ROUTER_ADD: {
		struct in6_addr *router_addr = (struct in6_addr *)cb->info;
		bool prefix_found = false;

		LOG_DBG("NET_EVENT_IPV6_ROUTER_ADD: iface %p, router %s", iface,
			net_addr_ntop(AF_INET6, router_addr, ipv6_addr_str, NET_IPV6_ADDR_LEN));

		ipv6_router_addr = *router_addr;

		/* Check that one of the iface public ipv6 addresses is having still
		 * the same prefix
		 */
		struct net_if_ipv6 *ipv6 = iface->config.ip.ipv6;

		ARRAY_FOR_EACH(ipv6->unicast, i)
		{
			struct in6_addr *ipv6_addr = &ipv6->unicast[i].address.in6_addr;

			if (!ipv6->unicast[i].is_used ||
			    ipv6->unicast[i].address.family != AF_INET6) {
				continue;
			}

			/* Check if this is a global address and has the same prefix as our
			 * sink prefix
			 */
			if (net_ipv6_is_global_addr(ipv6_addr) &&
			    net_ipv6_is_prefix(ipv6_addr->s6_addr,
					       sink_prefix_addr.s6_addr,
					       sizeof(struct in6_addr) / 2)) {
				prefix_found = true;
				break;
			}
		}
		if (prefix_found == false) {
			LOG_WRN("SINK: Router with IPv6 addr %s added to sink iface %p, but no "
				"existing public address with our prefix found - "
				"sink prefix needs to be re-created and we wait for "
				"NET_EVENT_IPV6_ADDR_ADD",
				net_addr_ntop(AF_INET6, router_addr, ipv6_addr_str,
					      NET_IPV6_ADDR_LEN),
				iface_for_prefix);
		} else if (sink_prefix_addr_set == false) {
			struct dect_sink_status_evt sink_status_data = {
				.sink_status = DECT_SINK_STATUS_CONNECTED,
				.br_iface = iface_for_prefix,
			};

			sink_prefix_addr_set = true;
			dect_mgmt_sink_status_evt(iface_for_dect, sink_status_data);
		}
		break;
	}
	case NET_EVENT_IPV6_ADDR_ADD: {
		struct net_event_ipv6_addr *evt_ipv6_addr = (struct net_event_ipv6_addr *)cb->info;
		struct in6_addr *ipv6_addr = &evt_ipv6_addr->addr;

		LOG_DBG("NET_EVENT_IPV6_ADDR_ADD: iface %p, addr %s", iface,
			net_addr_ntop(AF_INET6, ipv6_addr, ipv6_addr_str, NET_IPV6_ADDR_LEN));

		/* This is the trick: we get the 8 bytes as a prefix for
		 * dect nr+ network usage from 1st added public address.
		 */
		if (sink_prefix_addr_set == false && net_ipv6_is_global_addr(ipv6_addr)) {
			struct dect_sink_status_evt sink_status_data = {
				.sink_status = DECT_SINK_STATUS_CONNECTED,
				.br_iface = iface_for_prefix,
			};
			struct net_if_ipv6 *dect_ipv6s = iface_for_dect->config.ip.ipv6;

			/* So, we take 1st public address, and take 1st 64bits/8 bytes as
			 * a prefix for our usage
			 */
			memcpy(&sink_prefix_addr, ipv6_addr->s6_addr, 8);
			sink_prefix_addr_set = true;

			LOG_DBG("SINK: IPv6 addr %s/%d added for dect nr+ prefix usage",
				net_sprint_ipv6_addr(ipv6_addr), (sizeof(struct in6_addr) / 2) * 8);

			dect_net_l2_addr_util_global_addr_replace(iface_for_dect);

			/* Add also prefix to dect nr+ iface but remove current ones 1st */
			ARRAY_FOR_EACH(dect_ipv6s->prefix, i)
			{
				net_if_ipv6_prefix_rm(iface_for_dect,
					&dect_ipv6s->prefix[i].prefix,
					dect_ipv6s->prefix[i].len);
			}

			struct net_if_ipv6_prefix *prefix;

			prefix = net_if_ipv6_prefix_add(
				iface_for_dect,
				&sink_prefix_addr, 64, NET_IPV6_ND_INFINITE_LIFETIME);
			if (!prefix) {
				LOG_ERR("%s: cannot add IPv6 prefix to dect nr+ interface %p",
					(__func__), iface_for_dect);
			} else {
				LOG_DBG("SINK: IPv6 prefix %s/%d added to dect nr+ iface %p",
					net_sprint_ipv6_addr(&sink_prefix_addr), 64,
					iface_for_dect);
			}
			dect_mgmt_sink_status_evt(iface_for_dect, sink_status_data);
		}
		break;
	}
	case NET_EVENT_IPV6_ADDR_DEL: {
		struct net_event_ipv6_addr *evt_ipv6_addr = (struct net_event_ipv6_addr *)cb->info;
		struct in6_addr *ipv6_addr = &evt_ipv6_addr->addr;

		LOG_WRN("NET_EVENT_IPV6_ADDR_DEL: iface %p, addr %s", iface,
			net_addr_ntop(AF_INET6, ipv6_addr, ipv6_addr_str, NET_IPV6_ADDR_LEN));

		if (sink_prefix_addr_set == true &&
		    net_ipv6_is_prefix(ipv6_addr->s6_addr, sink_prefix_addr.s6_addr,
				       sizeof(struct in6_addr) / 2)) {
			struct dect_sink_status_evt sink_status_data = {
				.sink_status = DECT_SINK_STATUS_DISCONNECTED,
				.br_iface = iface_for_prefix,
			};

			LOG_WRN("SINK: IPv6 addr with our prefix %s/%d removed from iface %p",
				net_sprint_ipv6_addr(ipv6_addr), sizeof(struct in6_addr) / 2,
				iface);
			sink_prefix_addr_set = false;

			/* Remove also prefix that we set */
			if (!net_if_ipv6_prefix_rm(iface_for_dect, &sink_prefix_addr, 64)) {
				LOG_WRN("SINK: IPv6 prefix %s/64 removal failed from "
					"dect nr+ iface %p",
					net_sprint_ipv6_addr(&sink_prefix_addr), iface_for_dect);
			} else {
				LOG_INF("SINK: IPv6 prefix %s/64 removed from dect nr+ iface %p",
					net_sprint_ipv6_addr(&sink_prefix_addr), iface_for_dect);
			}
			dect_mgmt_sink_status_evt(iface_for_dect, sink_status_data);
		}
		break;
	}
	case NET_EVENT_IPV6_NBR_ADD: {
		struct net_event_ipv6_nbr *ipv6_nbr = (struct net_event_ipv6_nbr *)cb->info;

		LOG_DBG("NET_EVENT_IPV6_NBR_ADD: iface %p, nbr %s", iface,
			net_addr_ntop(AF_INET6, (struct in6_addr *)&ipv6_nbr->addr, ipv6_addr_str,
				      NET_IPV6_ADDR_LEN));
		break;
	}

	case NET_EVENT_IPV6_NBR_DEL: {
		struct net_event_ipv6_nbr *ipv6_nbr = (struct net_event_ipv6_nbr *)cb->info;

		LOG_DBG("NET_EVENT_IPV6_NBR_DEL: iface %p, nbr %s", iface,
			net_addr_ntop(AF_INET6, (struct in6_addr *)&ipv6_nbr->addr, ipv6_addr_str,
				      NET_IPV6_ADDR_LEN));
#if defined(CONFIG_MODEM_CELLULAR)
		/* It seems that LTE nw ipv6 router is removed from nbr table, let's add it back
		 * to keep connection open
		 */
		if (net_if_is_up(iface_for_prefix) &&
		    net_ipv6_addr_cmp(&ipv6_router_addr, (struct in6_addr *)&ipv6_nbr->addr)) {
			LOG_INF("NET_EVENT_IPV6_NBR_DEL: Sink IPv6 router removed "
				"as nbr - let's add it back");

			/* Submit a work to get it back (system queue) */
			k_work_reschedule(&lte_ipv6_router_nbr_deleted_work, K_MSEC(100));
		}
#endif
		break;
	}
	case NET_EVENT_IPV6_ROUTE_ADD:
		LOG_DBG("NET_EVENT_IPV6_ROUTE_ADD: iface %p", iface);
		break;
	case NET_EVENT_IPV6_ROUTE_DEL:
		LOG_DBG("NET_EVENT_IPV6_ROUTE_DEL: iface %p", iface);
		break;

	default:
		LOG_WRN("Unknown event %llu", mgmt_event);
		break;
	}
}

/**************************************************************************************************/

bool dect_net_l2_sink_ipv6_prefix_get(struct dect_net_l2_sink_ipv6_prefix *prefix_out)
{
	if (sink_prefix_addr_set == false || iface_for_prefix == NULL) {
		return false;
	}
	prefix_out->len = sizeof(struct in6_addr) / 2;
	net_ipaddr_copy(&prefix_out->prefix, &sink_prefix_addr);
	prefix_out->iface = iface_for_prefix;

	return true;
}

static struct net_mgmt_event_callback net_if_cb;

static void dect_net_l2_sink_net_if_mgmt_event_handler(struct net_mgmt_event_callback *cb,
						      uint64_t event, struct net_if *iface)
{
	if (iface != iface_for_prefix) {
		return;
	}

	switch (event) {
	case NET_EVENT_IF_UP: {
		struct dect_sink_status_evt sink_status_data = {
			.sink_status = DECT_SINK_STATUS_DISCONNECTED,
			.br_iface = iface_for_prefix,
		};

		LOG_INF("NET_EVENT_IF_UP: Sink networking iface is up");
		dect_mgmt_sink_status_evt(iface_for_dect, sink_status_data);

		/* TODO trigger router solicitation?
		 * i.e. handle case when already up and went down and up again
		 */
		break;
	}
	case NET_EVENT_IF_DOWN:
		struct dect_sink_status_evt sink_status_data = {
			.sink_status = DECT_SINK_STATUS_DISCONNECTED,
			.br_iface = iface_for_prefix,
		};

		LOG_WRN("NET_EVENT_IF_DOWN: Sink networking iface (%p) is down", iface_for_prefix);
		dect_mgmt_sink_status_evt(iface_for_dect, sink_status_data);
		sink_prefix_addr_set = false;

#if defined(CONFIG_MODEM_CELLULAR)
		struct net_if_ipv6 *ipv6 = iface->config.ip.ipv6;
		struct net_if_router *router;

		/* Work around: flush addresses and router as this is not done by cellular modem. */
		ARRAY_FOR_EACH(ipv6->unicast, i)
		{
			net_if_ipv6_addr_rm(iface, &ipv6->unicast[i].address.in6_addr);
		}
		router = net_if_ipv6_router_find_default(iface, NULL);
		if (router) {
			net_if_ipv6_router_rm(router);
		}

		memset(&ipv6_router_addr, 0, sizeof(ipv6_router_addr));
		net_ipv6_nbr_rm(iface, &router->address.in6_addr);
#endif
		/* Remove also prefix that we set */
		if (!net_if_ipv6_prefix_rm(iface_for_dect, &sink_prefix_addr, 64)) {
			LOG_WRN("SINK: IPv6 prefix %s/64 removal failed from dect nr+ iface %p",
				net_sprint_ipv6_addr(&sink_prefix_addr), iface_for_dect);
		} else {
			LOG_DBG("SINK: IPv6 prefix %s/64 removed from dect nr+ iface %p",
				net_sprint_ipv6_addr(&sink_prefix_addr), iface_for_dect);
		}

		/* TODO: We would need to stop/restart cluster to reset the prefix.
		 * TODO: talk with mdm team if could set/remove prefix anytime during
		 * cluster running?
		 * but until mdm got the support, setting to tell if we want to stop also cluster?
		 */
		break;
	default:
		break;
	}
}

#if defined(CONFIG_MODEM_CELLULAR)
static void dect_net_l2_sink_lte_ipv6_nbr_router_deleted_worker(struct k_work *work_item)
{
	struct net_route_entry *route;
	struct net_linkaddr lte_if_mac_addr;

	memset(lte_if_mac_addr.addr, 0, 6);
	lte_if_mac_addr.len = 6;
	lte_if_mac_addr.type = NET_LINK_UNKNOWN;

	/* Let's add router back */
	if (!net_ipv6_nbr_add(iface_for_prefix, &ipv6_router_addr, &lte_if_mac_addr, true,
			      NET_IPV6_NBR_STATE_REACHABLE)) {
		LOG_ERR("(%s): Cannot add IPv6 router as a nbr to sink prefix iface",
			(__func__));
	} else {
		LOG_DBG("(%s): sink iface IPv6 router added as a nbr to sink prefix iface",
			(__func__));
	}
	route = net_route_add(iface_for_prefix, &ipv6_router_addr, 128, &ipv6_router_addr,
			      NET_IPV6_ND_INFINITE_LIFETIME, NET_ROUTE_PREFERENCE_HIGH);
	if (!route) {
		LOG_ERR("Cannot add sink ipv6 router as a route");
	}
}
#endif

#define NET_IF_EVENT_MASK (NET_EVENT_IF_UP | NET_EVENT_IF_DOWN)
#define IPV6_LAYER_EVENT_MASK                                                                      \
	(NET_EVENT_IPV6_PREFIX_ADD | NET_EVENT_IPV6_PREFIX_DEL | NET_EVENT_IPV6_ADDR_ADD |         \
	 NET_EVENT_IPV6_ADDR_DEL | NET_EVENT_IPV6_ROUTER_ADD | NET_EVENT_IPV6_ROUTER_DEL |         \
	NET_EVENT_IPV6_NBR_DEL | NET_EVENT_IPV6_NBR_ADD | NET_EVENT_IPV6_ROUTE_ADD |               \
	 NET_EVENT_IPV6_ROUTE_DEL)

static int dect_net_l2_sink_init(void)
{
	iface_for_prefix = NULL;
	iface_for_dect = NULL;

	net_mgmt_init_event_callback(&dect_net_l2_net_mgmt_ipv6_event_cb,
				     dect_net_l2_net_mgmt_ipv6_event_handler,
				     IPV6_LAYER_EVENT_MASK);
	net_mgmt_init_event_callback(&net_if_cb, dect_net_l2_sink_net_if_mgmt_event_handler,
				     NET_IF_EVENT_MASK);
	net_mgmt_add_event_callback(&dect_net_l2_net_mgmt_ipv6_event_cb);
	net_mgmt_add_event_callback(&net_if_cb);
#if defined(CONFIG_NET_L2_ETHERNET)
	iface_for_prefix = net_if_get_first_by_type(&NET_L2_GET_NAME(ETHERNET));
	if (!iface_for_prefix) {
		LOG_ERR("No Ethernet interface found for sink");
		return -ENOENT;
	}
	LOG_INF("Ethernet interface found for sink");
#endif
	iface_for_dect = net_if_get_by_index(
		net_if_get_by_name("nrf91_dect")); /* TODO: kconfig for the used name */

	if (!iface_for_dect) {
		LOG_ERR("%s: interface nrf91_dect not found", (__func__));
	}

#if defined(CONFIG_MODEM_CELLULAR)
	struct net_if *const modem_iface = net_if_get_first_by_type(&NET_L2_GET_NAME(PPP));
	int ret;

	pm_device_action_run(modem, PM_DEVICE_ACTION_RESUME);

	ret = net_if_up(modem_iface);
	if (ret < 0) {
		printk("Failed to bring up modem interface\n");
		return -1;
	}
	iface_for_prefix = modem_iface;

	k_work_init_delayable(&lte_ipv6_router_nbr_deleted_work,
			      dect_net_l2_sink_lte_ipv6_nbr_router_deleted_worker);
#endif
#if defined(CONFIG_NET_CONNECTION_MANAGER)
	/* conn mgr does not have decent support for multiple interfaces for L4 events,
	 * thus we want to ignore the sink interface because we want that those
	 * are served for dect nr+ interface. TODO? we could have a config for this?
	 */
	if (iface_for_prefix) {
		conn_mgr_ignore_iface(iface_for_prefix);
	}
#endif
	return 0;
}

SYS_INIT(dect_net_l2_sink_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
