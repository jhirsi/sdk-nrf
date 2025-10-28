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

#include "dect_net_l2_ipv6.h"
#include "dect_net_l2_sink.h"
#include "dect_net_l2_internal.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(NET_L2_DECT, CONFIG_NET_L2_DECT_LOG_LEVEL);

#include "net_private.h" /* For net_sprint_ipv6_addr */

/**************************************************************************************************/

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

/**************************************************************************************************/

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
	ctx->ipv6_prefix_cfg.prefix_len = 0; /* No prefix by default */

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


/**************************************************************************************************/

void dect_net_l2_parent_association_created(
	struct net_if *iface, uint32_t target_long_rd_id,
	struct dect_net_ipv6_prefix_config *ipv6_prefix_config)
{
	bool found = false;

	LOG_DBG("dect_net_l2_parent_association_created: iface %p target_long_rd_id %u", iface,
		target_long_rd_id);

	for (int i = 0; i < ARRAY_SIZE(parent_associations); i++) {
		if (!parent_associations[i].in_use) {
			parent_associations[i].in_use = true;
			parent_associations[i].target_long_rd_id = target_long_rd_id;
			dect_net_l2_ipv6_addressing_parent_added_handle(
				&parent_associations[i], iface, target_long_rd_id,
				ipv6_prefix_config);
			dect_mgmt_parent_association_created_evt(iface, target_long_rd_id);

			/* We support only one parent */
			__ASSERT_NO_MSG(dect_net_l2_association_count_get() == 1);

			net_if_dormant_off(iface);

			dect_net_l2_join_ipv6_mdns_group(iface);

			found = true;
			break;
		}
	}
	__ASSERT_NO_MSG(found);
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
			dect_net_l2_ipv6_addressing_child_added_handle(
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
			dect_net_l2_ipv6_addressing_child_removed_handle(
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

			dect_net_l2_ipv6_parent_addressing_removed_handle(
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

void dect_net_l2_parent_ipv6_config_changed(
	struct net_if *iface, uint32_t parent_long_rd_id,
	struct dect_net_ipv6_prefix_config *ipv6_prefix_config)
{
	struct dect_net_l2_association_data *assoc_data =
		dect_net_l2_association_ref_get(parent_long_rd_id);

	if (assoc_data) {
		dect_net_l2_ipv6_addressing_parent_changed_handle(
			assoc_data, iface, parent_long_rd_id,
			ipv6_prefix_config);
	}
}

void dect_net_l2_sink_ipv6_config_changed(
	struct net_if *iface, struct dect_net_ipv6_prefix_config *ipv6_prefix_config)
{
	struct dect_net_l2_context *l2_ctx = net_if_l2_data(iface);
	bool update_global = false;

	__ASSERT_NO_MSG(iface);
	__ASSERT_NO_MSG(ipv6_prefix_config);
	__ASSERT_NO_MSG(l2_ctx);

	/* Update our addressing */
	update_global = dect_net_l2_ipv6_addressing_sink_changed_handle(
		iface, ipv6_prefix_config);

	/* Update children, by first removing all global nbrs */
	for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
		if (child_associations[i].in_use) {
			dect_net_l2_ipv6_global_addressing_child_removed_handle(
				&child_associations[i], iface);
		}
	}

	/* Then update / add global neigbors */
	if (update_global) {
		for (int i = 0; i < ARRAY_SIZE(child_associations); i++) {
			if (child_associations[i].in_use) {
				dect_net_l2_ipv6_global_addressing_child_changed_handle(
					l2_ctx, &child_associations[i], iface);
			}
		}
	}

}
