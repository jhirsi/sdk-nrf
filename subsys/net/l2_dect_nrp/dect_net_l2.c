/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_if.h>

#include "ipv6.h"

#include <zephyr/net/net_pkt.h>
#include <zephyr/net/mld.h>
#include <zephyr/net/dns_sd.h>

#include <net/dect_nrp_utils.h>

#include <dect_net_l2.h>
#include <dect_net_l2_mgmt.h>

#include "dect_net_l2_sink.h"
#include "dect_net_l2_internal.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(NET_L2_DECT, CONFIG_NET_L2_DECT_LOG_LEVEL);

#include "net_private.h"

/**************************************************************************************************/
struct dect_net_l2_association_data {
	bool in_use;
	uint32_t target_long_rd_id;

	bool local_ipv6_addr_set;
	struct in6_addr local_ipv6_addr;
	bool global_ipv6_addr_set;
	struct in6_addr global_ipv6_addr;
};

static struct dect_net_l2_association_data
	child_associations[CONFIG_DECT_NRP_MAC_CLUSTER_MAX_CHILD_ASSOCIATION_COUNT];
static struct dect_net_l2_association_data parent_associations[1]; /* TODO: magic */

static bool dect_net_l2_association_exists(uint32_t target_long_rd_id)
{
	for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
		if (child_associations[i].in_use &&
		    child_associations[i].target_long_rd_id == target_long_rd_id) {
			return true;
		}
	}
	for (int i = 0; i < ARRAY_SIZE(parent_associations); i++) {
		if (parent_associations[i].in_use &&
		    parent_associations[i].target_long_rd_id == target_long_rd_id) {
			return true;
		}
	}
	return false;
}

static bool dect_net_l2_association_parent_id_get(uint32_t *parent_long_rd_id_out,
						  enum dect_device_type device_type)
{
	if (device_type == DECT_DEVICE_TYPE_FT) {
		*parent_long_rd_id_out = 0;
		return false;
	}

	for (int i = 0; i < ARRAY_SIZE(parent_associations); i++) {
		if (parent_associations[i].in_use) {
			*parent_long_rd_id_out = parent_associations[i].target_long_rd_id;
			return true;
		}
	}
	return false;
}

static struct dect_net_l2_association_data *dect_net_l2_association_ref_get(
	uint32_t target_long_rd_id)
{
	for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
		if (child_associations[i].in_use &&
		    child_associations[i].target_long_rd_id == target_long_rd_id) {
			return &child_associations[i];
		}
	}
	for (int i = 0; i < ARRAY_SIZE(parent_associations); i++) {
		if (parent_associations[i].in_use &&
		    parent_associations[i].target_long_rd_id == target_long_rd_id) {
			return &parent_associations[i];
		}
	}
	return NULL;
}

static uint16_t dect_net_l2_association_count_get(void)
{
	uint16_t count = 0;

	for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
		if (child_associations[i].in_use) {
			count++;
		}
	}
	for (int i = 0; i < ARRAY_SIZE(parent_associations); i++) {
		if (parent_associations[i].in_use) {
			count++;
		}
	}
	return count;
}

void dect_net_l2_status_info_fill_association_data(
	struct net_if *iface, struct dect_status_info *status_info_out)
{
	if (status_info_out == NULL) {
		return;
	}
	struct dect_net_l2_association_data *ass_data = NULL;

	/* Fill parent data if available */
	for (int i = 0; i < status_info_out->parent_count; i++) {
		ass_data = dect_net_l2_association_ref_get(
			status_info_out->parent_associations[i].long_rd_id);
		if (ass_data) {
			status_info_out->parent_associations[i].local_ipv6_addr =
				ass_data->local_ipv6_addr;
			status_info_out->parent_associations[i].global_ipv6_addr_set =
				ass_data->global_ipv6_addr_set;
			if (ass_data->global_ipv6_addr_set) {
				status_info_out->parent_associations[i].global_ipv6_addr =
					ass_data->global_ipv6_addr;
			}
		}
	}
	/* Fill child data if available */
	for (int i = 0; i < status_info_out->child_count; i++) {
		ass_data = dect_net_l2_association_ref_get(
			status_info_out->child_associations[i].long_rd_id);
		if (ass_data) {
			status_info_out->child_associations[i].local_ipv6_addr =
				ass_data->local_ipv6_addr;
			status_info_out->child_associations[i].global_ipv6_addr_set =
				ass_data->global_ipv6_addr_set;
			if (ass_data->global_ipv6_addr_set) {
				status_info_out->child_associations[i].global_ipv6_addr =
					ass_data->global_ipv6_addr;
			}
		}
	}
}

void dect_net_l2_status_info_fill_sink_data(
	struct net_if *iface, struct dect_status_info *status_info_out)
{
	if (status_info_out == NULL) {
		return;
	}
	struct dect_net_l2_sink_ipv6_prefix l2_sink_prefix;
	struct in6_addr driver_ipv6_prefix = status_info_out->br_global_ipv6_addr_prefix;
	bool driver_ipv6_prefix_set = status_info_out->br_global_ipv6_addr_prefix_set;

	if (dect_net_l2_sink_ipv6_prefix_get(&l2_sink_prefix)) {
		struct in6_addr l2_sink_ipv6_prefix = l2_sink_prefix.prefix;

		/* Print warning if not the same as driver */
		if (!driver_ipv6_prefix_set ||
		    !net_ipv6_addr_cmp(&l2_sink_ipv6_prefix, &driver_ipv6_prefix)) {
			LOG_WRN("SINK: IPv6 prefix %s/%d does not match driver prefix %s/%d",
				net_sprint_ipv6_addr(&l2_sink_ipv6_prefix),
				(sizeof(struct in6_addr) / 2) * 8,
				net_sprint_ipv6_addr(&driver_ipv6_prefix),
				(sizeof(struct in6_addr) / 2) * 8);
		}
		/* Anyways, we report L2 sink information */
		status_info_out->br_net_iface = l2_sink_prefix.iface;
		status_info_out->br_global_ipv6_addr_prefix = l2_sink_ipv6_prefix;
		status_info_out->br_global_ipv6_addr_prefix_len = l2_sink_prefix.len;
		status_info_out->br_global_ipv6_addr_prefix_set = true;
	}
}

/**************************************************************************************************/
#include <zephyr/net/ethernet.h> /* just for ETH_P_ALL */

static enum net_verdict dect_net_l2_recv(struct net_if *iface, struct net_pkt *pkt)
{
	LOG_DBG("iface %p recv %d bytes from ipv6 addr %s", iface, net_pkt_get_len(pkt),
		net_sprint_ipv6_addr((struct in6_addr *)NET_IPV6_HDR(pkt)->src));

	/* Check if this possible ipv6 packet is targeted to one of our children,
	 * then we route directly from here
	 * OR
	 * In case of multicast, we forward to all children (except the one who sent this)
	 */
	uint8_t vtc_vhl = NET_IPV6_HDR(pkt)->vtc & 0xf0;
	int ret;

	if (vtc_vhl == 0x60) {
		if (net_ipv6_is_ll_addr((struct in6_addr *)NET_IPV6_HDR(pkt)->dst)) {
			uint32_t target_long_rd_id = dect_nrp_utils_long_rd_id_from_ipv6_addr(
				(struct in6_addr *)NET_IPV6_HDR(pkt)->dst);

			if (dect_net_l2_association_exists(target_long_rd_id)) {
				const struct dect_nrp_hal_api *api = net_if_get_device(iface)->api;

				if (!api) {
					LOG_ERR("Link local: no api for iface %p", iface);
					goto exit;
				}

				net_pkt_set_family(pkt, AF_INET6);

				/* Set ll addr for driver level to know destination long rd id */
				target_long_rd_id = htonl(target_long_rd_id);
				net_pkt_lladdr_dst(pkt)->len = sizeof(target_long_rd_id);
				memcpy(net_pkt_lladdr_dst(pkt)->addr, &target_long_rd_id,
				       net_pkt_lladdr_dst(pkt)->len);

				ret = net_l2_send(api->send, net_if_get_device(iface), iface, pkt);
				if (!ret) {
					ret = net_pkt_get_len(pkt);
					LOG_DBG("%s (iface %p): forwarded to long rd id %u "
						"(%d bytes)",
						(__func__), iface, ntohl(target_long_rd_id), ret);
				} else {
					LOG_ERR("%s: iface %p forwarding send error %d to "
						"long rd id %u",
						(__func__), iface, ret, ntohl(target_long_rd_id));
				}
				net_pkt_unref(pkt);

				return NET_OK; /* We handled this one... */
			}
		} else if (net_ipv6_is_addr_mcast_link((struct in6_addr *)NET_IPV6_HDR(pkt)->dst)) {
			/* link local scope multicast address (FFx2::) -> forward to all children */
			uint32_t source_long_rd_id = dect_nrp_utils_long_rd_id_from_ipv6_addr(
				(struct in6_addr *)NET_IPV6_HDR(pkt)->src);

			LOG_DBG("%s: IPv6 multicast packet to link local scope address",
				(__func__));

			/* Forward to all children */
			for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
				if (child_associations[i].in_use &&
				    child_associations[i].target_long_rd_id != source_long_rd_id) {
					const struct dect_nrp_hal_api *api =
						net_if_get_device(iface)->api;

					if (!api) {
						LOG_ERR("Multicast link local: no api for iface %p",
							iface);
						goto exit;
					}

					struct net_pkt *pkt_cpy = net_pkt_clone(pkt, K_NO_WAIT);
					uint32_t target_long_rd_id =
						child_associations[i].target_long_rd_id;

					if (pkt_cpy == NULL) {
						LOG_ERR("Cannot clone pkt");
						continue;
					}

					net_pkt_set_forwarding(pkt_cpy, true);
					net_pkt_set_orig_iface(pkt_cpy, pkt->iface);
					net_pkt_set_family(pkt_cpy, AF_INET6);
					net_pkt_set_iface(pkt_cpy, iface);

					/* Set ll addr for driver level to know destination long rd
					 * id
					 */
					target_long_rd_id = htonl(target_long_rd_id);
					net_pkt_lladdr_dst(pkt_cpy)->len =
						sizeof(target_long_rd_id);
					memcpy(net_pkt_lladdr_dst(pkt_cpy)->addr,
					       &target_long_rd_id,
					       net_pkt_lladdr_dst(pkt_cpy)->len);

					ret = net_l2_send(api->send, net_if_get_device(iface),
							  iface, pkt_cpy);
					if (ret) {
						LOG_ERR("%s: iface %p send error %d for multicast "
							"forward",
							(__func__), iface, ret);
						ret = net_pkt_get_len(pkt_cpy);
						net_pkt_unref(pkt_cpy);
					} else {
						ret = net_pkt_get_len(pkt_cpy);
						net_pkt_unref(pkt_cpy);
						LOG_DBG("%s (iface %p): multicast forwarded to "
							"long rd id %u (%d bytes)",
							(__func__), iface, ntohl(target_long_rd_id),
							ret);
					}
				}
			}

			/* ... and pass also to us */
		}
	}
exit:
	return NET_CONTINUE;
}

static int dect_net_l2_send(struct net_if *iface, struct net_pkt *pkt)
{
	const struct dect_nrp_hal_api *api = net_if_get_device(iface)->api;

	int ret = -1;
	uint32_t target_long_rd_id = 0;

	if (!api) {
		ret = -ENOENT;
		goto error;
	}

	if (!api->send) {
		ret = -ENOTSUP;
		goto error;
	}

	struct net_context *context;
	struct dect_net_l2_context *l2_ctx = net_if_l2_data(iface);
	enum dect_device_type device_type = l2_ctx->device_type;

	if (IS_ENABLED(CONFIG_NET_IPV6) && net_pkt_family(pkt) == AF_INET6) {
		uint32_t parent_long_rd_id = 0;

		target_long_rd_id = dect_nrp_utils_long_rd_id_from_ipv6_addr(
			(struct in6_addr *)NET_IPV6_HDR(pkt)->dst);

		if (!dect_net_l2_association_exists(target_long_rd_id)) {
			if (!dect_net_l2_association_parent_id_get(&parent_long_rd_id,
								   device_type)) {
				if (!(device_type == DECT_DEVICE_TYPE_FT &&
				    net_ipv6_is_addr_mcast_link(
					    (struct in6_addr *)NET_IPV6_HDR(pkt)->dst))) {
					LOG_WRN("%s: IPv6: no parent and no association with "
						"target long RD ID %u (parsed from ipv dst addr "
						"%s) - drop",
						(__func__), target_long_rd_id,
						net_sprint_ipv6_addr(
							(struct in6_addr *)NET_IPV6_HDR(pkt)->dst));
					ret = -EINVAL;
					goto error;
				}

				/* FT device: forward multicasts to all children */
				LOG_DBG("%s: FT device: forward multicast to all children",
					(__func__));
				for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
					if (child_associations[i].in_use == false) {
						continue;
					}
					struct net_pkt *pkt_cpy =
						net_pkt_clone(pkt, K_NO_WAIT);
					uint32_t target_long_rd_id =
						child_associations[i]
							.target_long_rd_id;
					if (pkt_cpy == NULL) {
						LOG_ERR("Cannot clone pkt");
						continue;
					}
					net_pkt_set_forwarding(pkt_cpy, true);
					net_pkt_set_orig_iface(pkt_cpy, pkt->iface);
					net_pkt_set_family(pkt_cpy, AF_INET6);
					net_pkt_set_iface(pkt_cpy, iface);
					/* Set ll addr for driver level to know
					 * destination long rd id
					 */
					target_long_rd_id =
						htonl(target_long_rd_id);
					net_pkt_lladdr_dst(pkt_cpy)->len =
						sizeof(target_long_rd_id);
					memcpy(net_pkt_lladdr_dst(pkt_cpy)->addr,
					       &target_long_rd_id,
					       net_pkt_lladdr_dst(pkt_cpy)->len);
					ret = net_l2_send(api->send,
							  net_if_get_device(iface),
							  iface, pkt_cpy);
					if (ret) {
						LOG_ERR("%s: iface %p send error "
							"%d for multicast "
							"forward",
							(__func__), iface, ret);
						net_pkt_unref(pkt_cpy);
					} else {
						ret = net_pkt_get_len(pkt_cpy);
						net_pkt_unref(pkt_cpy);
						LOG_DBG("%s (iface %p): multicast "
							"forwarded to "
							"long rd id %u (%d bytes)",
							(__func__), iface,
							ntohl(target_long_rd_id),
							ret);
					}
				}
				if (ret < 0) {
					LOG_WRN("%s: IPv6: multicast: no parent and no "
						"association with "
						"target long RD ID %u (parsed from ipv dst "
						"addr %s) - drop",
						(__func__), target_long_rd_id,
						net_sprint_ipv6_addr(
							(struct in6_addr *)NET_IPV6_HDR(pkt)
								->dst));
					ret = -EINVAL;
					goto error;
				}
				net_pkt_unref(pkt);
				return ret;
			}
			/* else: use parent long RD ID */
			target_long_rd_id = parent_long_rd_id;
		}
		LOG_DBG("IPv6 send: target_long_rd_id %u to ipv6 addr %s", target_long_rd_id,
			net_sprint_ipv6_addr((struct in6_addr *)NET_IPV6_HDR(pkt)->dst));

		/* Set ll addr for driver level to know destination long rd id */
		target_long_rd_id = htonl(target_long_rd_id);
		net_pkt_lladdr_dst(pkt)->len = sizeof(target_long_rd_id);
		memcpy(net_pkt_lladdr_dst(pkt)->addr, &target_long_rd_id,
		       net_pkt_lladdr_dst(pkt)->len);

		goto send;
	} else if (IS_ENABLED(CONFIG_NET_SOCKETS_PACKET) && net_pkt_family(pkt) == AF_PACKET) {
		enum net_sock_type socket_type;

		context = net_pkt_context(pkt);
		if (!context) {
			LOG_DBG("AF_PACKET: No context in the packet");
			ret = -EINVAL;
			goto error;
		}

		socket_type = net_context_get_type(context);
		if (socket_type == SOCK_DGRAM) {
			struct sockaddr_ll *dst_addr = (struct sockaddr_ll *)&context->remote;

			target_long_rd_id =
				dect_nrp_utils_dst_long_rd_id_get_from_dst_sock_ll_addr(dst_addr);
			if (dect_net_l2_association_exists(target_long_rd_id)) {
				struct sockaddr_ll_ptr *src_addr =
					(struct sockaddr_ll_ptr *)&context->local;

				/* Set destination to pkt for the driver level */
				__ASSERT_NO_MSG(dst_addr->sll_halen <=
						sizeof(net_pkt_lladdr_dst(pkt)->addr));

				memcpy(net_pkt_lladdr_dst(pkt)->addr, dst_addr->sll_addr,
				       dst_addr->sll_halen);
				net_pkt_lladdr_dst(pkt)->len = dst_addr->sll_halen;

				__ASSERT_NO_MSG(src_addr->sll_halen <=
						sizeof(net_pkt_lladdr_src(pkt)->addr));
				memcpy(net_pkt_lladdr_src(pkt)->addr, src_addr->sll_addr,
				       src_addr->sll_halen);
				net_pkt_lladdr_src(pkt)->len = src_addr->sll_halen;

				goto send;
			} else {
				LOG_WRN("No association with target %u - drop", target_long_rd_id);
				ret = -EINVAL;
				goto error;
			}

			/* Send the packet as it is */
			goto send;
		} else {
			/* Others not supported */
			LOG_ERR("Socket type %d not supported", socket_type);
			ret = -EINVAL;
			goto error;
		}
	} else {
		/* Others not yet supported */
		LOG_ERR("Packet type %d not supported", net_pkt_family(pkt));
		ret = -EINVAL;
		goto error;
	}
send:
	ret = net_l2_send(api->send, net_if_get_device(iface), iface, pkt);
	if (ret) {
		goto error;
	}
	ret = net_pkt_get_len(pkt);
	net_pkt_unref(pkt);

	LOG_DBG("iface %p sent %d bytes (caller %p)", iface, ret, __builtin_return_address(0));

	return ret;
error:
	LOG_ERR("%s: iface %p send error %d", (__func__), iface, ret);
	return ret;
}

static int dect_net_l2_enable(struct net_if *iface, bool state)
{
	LOG_DBG("%s: iface %p %s", (__func__), iface, state ? "up" : "down");

	return 0;
}

static enum net_l2_flags dect_net_l2_flags(struct net_if *iface)
{
	struct dect_net_l2_context *ctx = net_if_l2_data(iface);

	return ctx->flags;
}

NET_L2_INIT(DECT_L2, dect_net_l2_recv, dect_net_l2_send, dect_net_l2_enable, dect_net_l2_flags);

void dect_net_l2_init(struct net_if *iface, struct dect_settings *initial_settings)
{
	struct dect_net_l2_context *ctx = net_if_l2_data(iface);
	struct net_if_addr *ifaddr;
	struct in6_addr iid;

	LOG_DBG("Initializing DECT L2 %p for iface %d (%p)", ctx, net_if_get_by_iface(iface),
		iface);

	net_if_flag_set(iface, NET_IF_IPV6);
	net_if_flag_set(iface, NET_IF_IPV6_NO_ND);
	net_if_dormant_on(iface);
	ctx->network_id = initial_settings->identities.network_id;
	ctx->transmitter_long_rd_id = initial_settings->identities.transmitter_long_rd_id;
	ctx->device_type = initial_settings->device_type;
	ctx->ipv6_prefix_cfg.type = DECT_MAC_IPV6_ADDRESS_TYPE_NONE;

	/* Add link-local address */
	dect_nrp_utils_net_ipv6_addr_create_iid(&iid, net_if_get_link_addr(iface));
	ifaddr = net_if_ipv6_addr_add(iface, &iid, NET_ADDR_AUTOCONF, 0);
	if (!ifaddr) {
		LOG_ERR("%s: cannot add address to interface %p", (__func__), iface);
	} else {
		/* As DAD is disabled in dect net iface,
		 * we need to mark the address as a preferred one.
		 */
		ifaddr->addr_state = NET_ADDR_PREFERRED;
		ctx->local_ipv6_addr = iid;
		/* TODO: set initial_local_ipv6_addr */
	}
}

static void dect_net_l2_join_ipv6_mdns_group(struct net_if *iface)
{
	struct in6_addr ipv6mr_multiaddr;
	int ret;

	/* Well known IPv6 ff02::fb address */
	net_ipv6_addr_create(&ipv6mr_multiaddr, 0xff02, 0, 0, 0, 0, 0, 0, 0x00fb);

	ret = net_ipv6_mld_join(iface, &ipv6mr_multiaddr);
	if (ret < 0) {
		LOG_DBG("Iface %p, cannot join mDNS (ff02::fb) IPv6 multicast group (%d)", iface,
			ret);
	} else {
		LOG_DBG("Iface %p, joined mDNS (ff02::fb) IPv6 multicast group", iface);
	}
}

/**************************************************************************************************/

#if defined(CONFIG_DATE_TIME_NTP)
#include <date_time.h>
static void date_time_event_handler(const struct date_time_evt *evt)
{
	switch (evt->type) {
	case DATE_TIME_OBTAINED_MODEM:
		LOG_INF("DATE_TIME_OBTAINED_MODEM");
		break;
	case DATE_TIME_OBTAINED_NTP:
		LOG_INF("DATE_TIME_OBTAINED_NTP");
		break;
	case DATE_TIME_OBTAINED_EXT:
		LOG_INF("DATE_TIME_OBTAINED_EXT");
		break;
	case DATE_TIME_NOT_OBTAINED:
		LOG_INF("DATE_TIME_NOT_OBTAINED");
		break;
	default:
		break;
	}
}
#endif

static bool dect_net_l2_util_ipv6_link_local_addr_create_add(
	struct net_if *iface, struct in6_addr *link_local_addr_out)
{
	struct net_if_addr *ifaddr;
	struct in6_addr iid;

	dect_nrp_utils_net_ipv6_addr_create_iid(&iid, net_if_get_link_addr(iface));
	ifaddr = net_if_ipv6_addr_add(iface, &iid, NET_ADDR_AUTOCONF, 0);
	if (!ifaddr) {
		LOG_WRN("%s: cannot add link address to interface %p", (__func__), iface);
		return false;
	}
	LOG_DBG("Link local IPv6 address %s added to interface %p",
		net_sprint_ipv6_addr(&iid), iface);
	*link_local_addr_out = iid;
	return true;
}


static bool dect_net_l2_util_ipv6_global_addr_create_add(
	struct net_if *iface, struct dect_mac_ipv6_address_config *ipv6_addr_cfg,
	struct in6_addr *global_ipv6_addr_out)
{
	struct in6_addr global_addr = {};
	struct in6_addr prefix_local = {};
	struct net_if_addr *ifaddr;
	bool added = false;

	UNALIGNED_PUT(htonl(0xfe800000), &prefix_local.s6_addr32[0]);
	/* Set ipv6 addr based on given info from peer FT device */
	if (ipv6_addr_cfg->type == DECT_MAC_IPV6_ADDRESS_TYPE_NONE) {
		LOG_INF("No IPv6 address to set - using link local only");
	} else {
		int len = (ipv6_addr_cfg->type == DECT_MAC_IPV6_ADDRESS_TYPE_FULL)
				  ? 16
				  : 8;

		/* Create our own IPv6 address using the given prefix and iid. We first
		 * setup link local address, and then copy prefix over first 16/8
		 * bytes of that address.
		 */
		dect_nrp_utils_net_ipv6_addr_create_iid(&global_addr, net_if_get_link_addr(iface));
		memcpy(&global_addr.s6_addr, ipv6_addr_cfg->address, len);

		ifaddr = net_if_ipv6_addr_lookup(&global_addr, NULL);
		if (ifaddr) {
			LOG_WRN("IPv6 address %s already exists - continue",
				net_sprint_ipv6_addr(&global_addr));
			net_if_addr_set_lf(ifaddr, true);
		} else {
			ifaddr = net_if_ipv6_addr_add(iface, &global_addr, NET_ADDR_AUTOCONF, 0);
			if (!ifaddr) {
				LOG_WRN("%s: cannot add address (%s) to interface %p", (__func__),
					net_sprint_ipv6_addr(&global_addr), iface);
			} else {
				added = true;
				*global_ipv6_addr_out = global_addr;
				LOG_INF("Global IPv6 address %s added to interface %p",
					net_sprint_ipv6_addr(&global_addr), iface);
			}
		}
	}
	return added;
}

#if defined(CONFIG_NET_IPV6_NBR_CACHE)
static void dect_net_l2_util_ipv6_nbr_add(
	struct net_if *iface, struct dect_mac_ipv6_address_config *ipv6_prefix_cfg,
	uint32_t sink_long_rd_id, uint32_t nbr_long_rd_id,
	bool *nbr_local_addr_was_set, struct in6_addr *nbr_local_ipv6_addr_out,
	bool *nbr_global_addr_was_set, struct in6_addr *nbr_global_ipv6_addr_out)
{
	bool nbr_addr_generated;
	struct in6_addr nbr_addr = {};
	struct in6_addr prefix = {};

	__ASSERT_NO_MSG(nbr_local_addr_was_set != NULL && nbr_global_addr_was_set != NULL);
	__ASSERT_NO_MSG(nbr_local_ipv6_addr_out != NULL && nbr_global_ipv6_addr_out != NULL);

	UNALIGNED_PUT(htonl(0xfe800000), &prefix.s6_addr32[0]);

	/* Add local addr as a neighbor */
	nbr_addr_generated = dect_nrp_utils_net_ipv6_addr_create_from_sink_and_long_rd_id(
		prefix, sink_long_rd_id, nbr_long_rd_id,
		&nbr_addr);
	if (nbr_addr_generated) {
		/* local: add a parent as a neighbor to dect iface */
		if (!net_ipv6_nbr_add(iface, &nbr_addr, net_if_get_link_addr(iface), false,
				      NET_IPV6_NBR_STATE_REACHABLE)) {
			LOG_ERR("(%s): cannot add parents local addr (%s) as nbr to dect iface",
				(__func__), net_sprint_ipv6_addr(&nbr_addr));
		} else {
			*nbr_local_addr_was_set = true;
			*nbr_local_ipv6_addr_out = nbr_addr;
			LOG_INF("(%s): long RD ID %u, local addr %s (link addr %s) "
				"added as a neighbor to dect iface %p",
				(__func__), nbr_long_rd_id, net_sprint_ipv6_addr(&nbr_addr),
				net_sprint_ll_addr(net_if_get_link_addr(iface)->addr, 8), iface);
		}
	} else {
		LOG_ERR("(%s): cannot create parents local addr as nbr to dect iface",
			(__func__));
	}
	if (ipv6_prefix_cfg->type != DECT_MAC_IPV6_ADDRESS_TYPE_NONE) {
		int len = (ipv6_prefix_cfg->type == DECT_MAC_IPV6_ADDRESS_TYPE_FULL)
				  ? 16
				  : 8;

		memcpy(&prefix, ipv6_prefix_cfg->address, len);

		/* Add global addr as a neighbor */
		nbr_addr_generated = dect_nrp_utils_net_ipv6_addr_create_from_sink_and_long_rd_id(
			prefix, sink_long_rd_id, nbr_long_rd_id, &nbr_addr);
		if (nbr_addr_generated) {
			/* global: add a parent as a neighbor to dect iface */
			if (!net_ipv6_nbr_add(iface, &nbr_addr, net_if_get_link_addr(iface), false,
					      NET_IPV6_NBR_STATE_REACHABLE)) {
				LOG_ERR("(%s): cannot add parents global addr as nbr to dect iface",
					(__func__));
			} else {
				*nbr_global_addr_was_set = true;
				*nbr_global_ipv6_addr_out = nbr_addr;
				LOG_DBG("(%s): global addr %s (link addr %s) added "
					"as a neighbor to dect iface",
					(__func__), net_sprint_ipv6_addr(&nbr_addr),
					net_sprint_ll_addr(net_if_get_link_addr(iface)->addr, 8));
			}
		}
	}
}

static void dect_net_l2_util_ipv6_nbr_remove(
	struct net_if *iface,
	struct dect_net_l2_association_data *ass_list_item)
{
	if (ass_list_item->local_ipv6_addr_set) {
		if (!net_ipv6_nbr_rm(iface, &ass_list_item->local_ipv6_addr)) {
			LOG_WRN("Failed to remove local IPv6 neighbor %s on iface %p",
				net_sprint_ipv6_addr(&ass_list_item->local_ipv6_addr), iface);
		}
	}
	if (ass_list_item->global_ipv6_addr_set) {
		if (!net_ipv6_nbr_rm(iface, &ass_list_item->global_ipv6_addr)) {
			LOG_ERR("Failed to remove global IPv6 neighbor %s on iface %p",
				net_sprint_ipv6_addr(&ass_list_item->global_ipv6_addr), iface);
		}
	}
}
#endif

static void dect_net_l2_util_parent_added_ipv6_addressing_handle(
	struct dect_net_l2_association_data *list_item,
	struct net_if *iface, uint32_t parent_long_rd_id,
	struct dect_mac_ipv6_address_config *ipv6_addr_cfg)
{
	bool removed = false;
	struct dect_net_l2_context *ctx = net_if_l2_data(iface);

	/* Store prefix config */
	ctx->ipv6_prefix_cfg = *ipv6_addr_cfg;

	/* Parent added: remove/update our link local addr and ipv6 IID */
	removed = net_if_ipv6_addr_rm(iface, &ctx->local_ipv6_addr);
	if (!removed) {
		LOG_ERR("Failed to remove local IPv6 address %s on iface %p",
			net_sprint_ipv6_addr(&ctx->local_ipv6_addr), iface);
	}

	/* Link level addr has been set by the driver according
	 * to parent/sink long rd id + our long rd id.
	 * Let's continue from that on IPv6 level.
	 * Let's add our link local addr and possible global address to net iface
	 * and also add parent as a neighbor.
	 */
	if (!dect_net_l2_util_ipv6_link_local_addr_create_add(iface, &ctx->local_ipv6_addr)) {
		LOG_WRN("%s: cannot add our link local address to interface %p",
			(__func__), iface);
	}

	ctx->global_ipv6_addr_set = dect_net_l2_util_ipv6_global_addr_create_add(
		iface, ipv6_addr_cfg,
		&ctx->global_ipv6_addr);

#if defined(CONFIG_NET_IPV6_NBR_CACHE)
	/* Add parent as a neighbor and also in association list as nbr */
	dect_net_l2_util_ipv6_nbr_add(iface, ipv6_addr_cfg,
					  parent_long_rd_id,
					  parent_long_rd_id,
					  &list_item->local_ipv6_addr_set,
					  &list_item->local_ipv6_addr,
					  &list_item->global_ipv6_addr_set,
					  &list_item->global_ipv6_addr);
#endif
}

void dect_net_l2_parent_association_created(
	struct net_if *iface, uint32_t target_long_rd_id,
	struct dect_mac_ipv6_address_config ipv6_addr_cfg)
{
	bool found = false;

	LOG_DBG("dect_net_l2_parent_association_created: iface %p target_long_rd_id %u", iface,
		target_long_rd_id);

	for (int i = 0; i < ARRAY_SIZE(parent_associations); i++) {
		if (!parent_associations[i].in_use) {
			parent_associations[i].in_use = true;
			parent_associations[i].target_long_rd_id = target_long_rd_id;
			dect_net_l2_util_parent_added_ipv6_addressing_handle(
				&parent_associations[i], iface, target_long_rd_id, &ipv6_addr_cfg);
			dect_mgmt_parent_association_created_evt(iface, target_long_rd_id);

			/* We support only one parent */
			__ASSERT_NO_MSG(dect_net_l2_association_count_get() == 1);

			net_if_dormant_off(iface);

			dect_net_l2_join_ipv6_mdns_group(iface);
#if defined(CONFIG_DATE_TIME_NTP)
			/* Get time over NTP */
			date_time_update_async(date_time_event_handler);
#endif

			found = true;
			break;
		}
	}
	__ASSERT_NO_MSG(found);
}

static void dect_net_l2_util_child_added_ipv6_addressing_handle(
	struct dect_net_l2_association_data *ass_list_item, struct net_if *iface,
	uint32_t child_long_rd_id, bool first_child)
{
	struct dect_net_l2_context *ctx = net_if_l2_data(iface);

	/* Add child as a neighbor, both local and global */
	if (first_child) {
		/* But 1st,
		 * update ipv6 prefix info, and in case if there was related settings changes,
		 * so update also ipv6 addressing also for this device
		 * when 1st association is created.
		 */
		bool done = net_if_ipv6_addr_rm(iface, &ctx->local_ipv6_addr);

		if (!done) {
			LOG_WRN("%s: cannot remove our local address %s from interface %p",
				(__func__), net_sprint_ipv6_addr(&ctx->local_ipv6_addr), iface);
		}
		if (!dect_net_l2_util_ipv6_link_local_addr_create_add(iface,
								      &ctx->local_ipv6_addr)) {
			LOG_WRN("%s: cannot add our link local address to interface %p", (__func__),
				iface);
		}
		/* Update also our global address */
		dect_net_l2_addr_util_global_addr_replace(iface);
	}

#if defined(CONFIG_NET_IPV6_NBR_CACHE)
	/* Add child as a neighbor and also in association list */
	dect_net_l2_util_ipv6_nbr_add(
		iface, &ctx->ipv6_prefix_cfg, ctx->transmitter_long_rd_id, child_long_rd_id,
		&ass_list_item->local_ipv6_addr_set, &ass_list_item->local_ipv6_addr,
		&ass_list_item->global_ipv6_addr_set, &ass_list_item->global_ipv6_addr);
#endif
}

void dect_net_l2_child_association_created(struct net_if *iface, uint32_t child_long_rd_id)
{
	bool first_child = false;

	LOG_DBG("dect_net_l2_child_association_created: iface %p child_long_rd_id %u", iface,
		child_long_rd_id);

	for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
		if (!child_associations[i].in_use) {
			child_associations[i].in_use = true;
			child_associations[i].target_long_rd_id = child_long_rd_id;

			first_child = (dect_net_l2_association_count_get() == 1);
			dect_net_l2_util_child_added_ipv6_addressing_handle(
				&child_associations[i], iface, child_long_rd_id, first_child);
			dect_mgmt_child_association_created_evt(iface, child_long_rd_id);

			/* Put the carrier on if this was the first association */
			if (first_child) {
				net_if_dormant_off(iface);
				dect_net_l2_join_ipv6_mdns_group(iface);
			}
			return;
		}
	}
}

static bool dect_net_l2_no_associations(void)
{
	for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
		if (child_associations[i].in_use) {
			return false;
		}
	}
	for (int i = 0; i < ARRAY_SIZE(parent_associations); i++) {
		if (parent_associations[i].in_use) {
			return false;
		}
	}
	return true;
}

static void dect_net_l2_util_parent_removed_ipv6_addressing_handle(
	struct dect_net_l2_association_data *ass_list_item,
	struct net_if *iface, uint32_t parent_long_rd_id)
{
	struct dect_net_l2_context *ctx = net_if_l2_data(iface);
	bool removed;

#if defined(CONFIG_NET_IPV6_NBR_CACHE)
	dect_net_l2_util_ipv6_nbr_remove(
		iface,
		ass_list_item);
#endif

	removed = net_if_ipv6_addr_rm(iface, &ctx->local_ipv6_addr);
	if (!removed) {
		LOG_WRN("%s: cannot remove our local address %s from interface %p",
			(__func__), net_sprint_ipv6_addr(&ctx->local_ipv6_addr), iface);
	}

	/* Set link local addr back as original */
	if (!dect_net_l2_util_ipv6_link_local_addr_create_add(iface, &ctx->local_ipv6_addr)) {
		LOG_WRN("%s: cannot add our orig link local address to interface %p",
			(__func__), iface);
	}

	/* Our local address was already updated, now remove our global IP address */
	if (ctx->global_ipv6_addr_set) {
		net_if_ipv6_addr_rm(iface, &ctx->global_ipv6_addr);
	}
	ctx->global_ipv6_addr_set = false;
	ass_list_item->global_ipv6_addr_set = false;
	ass_list_item->local_ipv6_addr_set = false;
}

static void dect_net_l2_util_child_removed_ipv6_addressing_handle(
	struct dect_net_l2_association_data *ass_list_item,
	struct net_if *iface, uint32_t child_long_rd_id)
{
	if (ass_list_item == NULL) {
		return;
	}
#if defined(CONFIG_NET_IPV6_NBR_CACHE)
	dect_net_l2_util_ipv6_nbr_remove(
		iface,
		ass_list_item);
#endif
	ass_list_item->global_ipv6_addr_set = false;
	ass_list_item->local_ipv6_addr_set = false;
}

void dect_net_l2_association_removed(
	struct net_if *iface, uint32_t long_rd_id,
	enum dect_association_release_cause cause, bool neighbor_initiated)
{
	LOG_DBG("dect_net_l2_association_removed: iface %p, "
		"long_rd_id %u, release_cause %d", iface, long_rd_id, cause);

	for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
		if (child_associations[i].in_use &&
		    child_associations[i].target_long_rd_id == long_rd_id) {
			child_associations[i].in_use = false;
			dect_net_l2_util_child_removed_ipv6_addressing_handle(
				&child_associations[i], iface, long_rd_id);
			dect_mgmt_association_released_evt(
				iface, long_rd_id, DECT_NEIGHBOR_ROLE_CHILD,
				neighbor_initiated, cause);

			/* If this was the last association, drop the carrier */
			if (dect_net_l2_no_associations()) {
				net_if_dormant_on(iface);
			}
			return;
		}
	}
	for (int i = 0; i < ARRAY_SIZE(parent_associations); i++) {
		if (parent_associations[i].in_use &&
		    parent_associations[i].target_long_rd_id == long_rd_id) {
			parent_associations[i].in_use = false;

			dect_net_l2_util_parent_removed_ipv6_addressing_handle(
				&parent_associations[i], iface, long_rd_id);
			dect_mgmt_association_released_evt(
				iface, long_rd_id, DECT_NEIGHBOR_ROLE_PARENT,
				neighbor_initiated, cause);

			/* If this was the last association, drop the carrier */
			if (dect_net_l2_no_associations()) {
				net_if_dormant_on(iface);
			}
			return;
		}
	}
}

void dect_net_l2_settings_changed(
	struct net_if *iface, struct dect_settings *driver_current_settings)
{
	struct dect_net_l2_context *ctx = net_if_l2_data(iface);

	/* Update needed settings in our side to context.
	 * Addressing are updated on next time when 1st association is created.
	 */
	ctx->network_id = driver_current_settings->identities.network_id;
	ctx->transmitter_long_rd_id = driver_current_settings->identities.transmitter_long_rd_id;
	ctx->device_type = driver_current_settings->device_type;
}

#if RM_JH /* TODO */
void dect_net_l2_parent_ipv6_config_changed(
	struct net_if *iface, uint32_t parent_long_rd_id,
	struct dect_mac_ipv6_address_config ipv6_addr_cfg)
{
	LOG_DBG("dect_net_l2_parent_ipv6_config_changed: iface %p, parent_long_rd_id %u",
		iface, parent_long_rd_id);

}
#endif

void dect_net_l2_addr_util_global_addr_replace(struct net_if *dect_iface)
{
	struct dect_net_l2_context *ctx = net_if_l2_data(dect_iface);
	struct net_if_ipv6 *dect_ipv6s = dect_iface->config.ip.ipv6;
	bool add_also_global = false;
	struct dect_net_l2_sink_ipv6_prefix sink_global_prefix;

	if (dect_net_l2_sink_ipv6_prefix_get(&sink_global_prefix)) {
		memcpy(&ctx->ipv6_prefix_cfg.address, sink_global_prefix.prefix.s6_addr,
		       sink_global_prefix.len);
		__ASSERT_NO_MSG(sink_global_prefix.len == 8);
		ctx->ipv6_prefix_cfg.type = DECT_MAC_IPV6_ADDRESS_TYPE_PREFIX;
		add_also_global = true;
	} else {
		ctx->ipv6_prefix_cfg.type = DECT_MAC_IPV6_ADDRESS_TYPE_NONE;
	}

	/* Remove all old global address from dect nr+ iface*/
	ARRAY_FOR_EACH(dect_ipv6s->unicast, i)
	{
		if (net_ipv6_is_global_addr(&dect_ipv6s->unicast[i].address.in6_addr)) {
			LOG_DBG("Removing old global address %s",
				net_sprint_ipv6_addr(&dect_ipv6s->unicast[i].address.in6_addr));
			net_if_ipv6_addr_rm(dect_iface,
					    &dect_ipv6s->unicast[i].address.in6_addr);
		}
	}
#if RM_JH
	/* Remove all old prefixes */
	if (ctx->global_ipv6_addr_set) {
		ARRAY_FOR_EACH(dect_ipv6s->prefix, i)
		{
			net_if_ipv6_prefix_rm(dect_iface,
				&dect_ipv6s->prefix[i].prefix,
				dect_ipv6s->prefix[i].len);
		}
		ctx->global_ipv6_addr_set = false;
	}
#endif
	/* ...and finally set new global address */
	ctx->global_ipv6_addr_set = dect_net_l2_util_ipv6_global_addr_create_add(
		dect_iface, &ctx->ipv6_prefix_cfg,
		&ctx->global_ipv6_addr);

	/* TODO handle case when prefix changed */
}
