/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_if.h>

#include <zephyr/net/net_pkt.h>
#include <zephyr/net/mld.h>

#include <net/dect_nrp_utils.h>

#include <dect_net_l2.h>
#include <dect_net_l2_mgmt.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(NET_L2_DECT, CONFIG_NET_L2_DECT_LOG_LEVEL);

#include "net_private.h"

/**************************************************************************************************/
struct dect_net_l2_association_data {
	bool in_use;
	uint32_t target_long_rd_id;
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

			LOG_INF("%s: IPv6 multicast packet to link local scope address",
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
						LOG_INF("%s (iface %p): multicast forwarded to "
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
				LOG_INF("%s: FT device: forward multicast to all children",
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
						LOG_INF("%s (iface %p): multicast "
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

	LOG_DBG("Initializing DECT L2 %p for iface %d (%p)", ctx, net_if_get_by_iface(iface),
		iface);

	net_if_flag_set(iface, NET_IF_IPV6);
	net_if_flag_set(iface, NET_IF_IPV6_NO_ND);
	net_if_dormant_on(iface);
#if RM_JH
	/* Allocate room for children */
	child_associations = k_calloc(initial_settings->cluster_beacon.max_num_neighbors,
				      sizeof(struct dect_net_l2_association_data));
	if (!child_associations) {
		LOG_ERR("%s: cannot allocate child associations", (__func__));
		LOG_WRN("Cannot continue, no memory - "
			"reduce amount of cluster neigbors and reboot");
	}
#endif
	/* Note: TODO? these are update only after a bootup! */
	ctx->network_id = initial_settings->identities.network_id;
	ctx->transmitter_long_rd_id = initial_settings->identities.transmitter_long_rd_id;
	ctx->device_type = initial_settings->device_type;
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

static void dect_net_l2_join_ipv6_mdns_group(struct net_if *iface)
{
	struct in6_addr ipv6mr_multiaddr;
	int ret;

	/* Well known IPv6 ff02::fb address */
	net_ipv6_addr_create(&ipv6mr_multiaddr, 0xff02, 0, 0, 0, 0, 0, 0, 0x00fb);

	ret = net_ipv6_mld_join(iface, &ipv6mr_multiaddr);
	if (ret < 0) {
		LOG_WRN("Iface %p, cannot join mDNS (ff02::fb) IPv6 multicast group (%d)", iface,
			ret);
	} else {
		LOG_INF("Iface %p, joined mDNS (ff02::fb) IPv6 multicast group", iface);
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

void dect_net_l2_parent_association_created(struct net_if *iface, uint32_t target_long_rd_id)
{
	bool found = false;

	LOG_DBG("dect_net_l2_parent_association_created: iface %p target_long_rd_id %u", iface,
		target_long_rd_id);

	for (int i = 0; i < ARRAY_SIZE(parent_associations); i++) {
		if (!parent_associations[i].in_use) {
			parent_associations[i].in_use = true;
			parent_associations[i].target_long_rd_id = target_long_rd_id;
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

#include <zephyr/net/dns_sd.h>
void dect_net_l2_child_association_created(struct net_if *iface, uint32_t target_long_rd_id)
{
	LOG_DBG("dect_net_l2_child_association_created: iface %p target_long_rd_id %u", iface,
		target_long_rd_id);

	for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
		if (!child_associations[i].in_use) {
			child_associations[i].in_use = true;
			child_associations[i].target_long_rd_id = target_long_rd_id;
			dect_mgmt_child_association_created_evt(iface, target_long_rd_id);

			/* Put the carrier on if this was the first association */
			if (dect_net_l2_association_count_get() == 1) {
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

void dect_net_l2_association_removed(struct net_if *iface, uint32_t long_rd_id)
{
	LOG_DBG("dect_net_l2_association_removed: iface %p long_rd_id %u", iface, long_rd_id);
	for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
		if (child_associations[i].in_use &&
		    child_associations[i].target_long_rd_id == long_rd_id) {
			child_associations[i].in_use = false;
			dect_mgmt_association_released_evt(iface, long_rd_id);

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
			dect_mgmt_association_released_evt(iface, long_rd_id);

			/* If this was the last association, drop the carrier */
			if (dect_net_l2_no_associations()) {
				net_if_dormant_on(iface);
			}
			return;
		}
	}
}
