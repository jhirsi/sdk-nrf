

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>

#include "ipv6.h"

#include <net/dect_nrp_utils.h>

#if defined(CONFIG_MODEM_INFO)
#include <modem/modem_info.h>
#endif

#include "dect_net_l2.h"
#include "dect_net_l2_mgmt.h"

#include "dect_nrf91_ctrl.h"
#include "dect_nrf91_settings.h"
#include "dect_nrf91_sink.h"
#include "dect_nrf91.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(DECT_NRP_MAC, CONFIG_DECT_NRP_MAC_LOG_LEVEL);

#include "net_private.h" /* for net_sprint_ipv6_addr */

struct dect_nrf91_association_data {
	bool in_use;
	uint32_t target_long_rd_id;

	/* TODO: to be removed? */
	struct in6_addr local_ipv6_addr;

	bool global_ipv6_addr_set;
	struct in6_addr global_ipv6_addr;
};

struct dect_nrf91_mac_dev_context {
	struct net_if *iface;
	uint8_t link_addr[8];
	uint32_t parent_long_rd_id;

	struct dect_nrf91_association_data
		child_associations[CONFIG_DECT_NRP_MAC_CLUSTER_MAX_CHILD_ASSOCIATION_COUNT];
	struct dect_nrf91_association_data parent_associations[1]; /* TODO: magic */

	/* PT/leaf device global addr TODO: is these really needed here? */
	bool global_ipv6_addr_set;
	struct in6_addr global_ipv6_addr;
	struct in6_addr local_ipv6_addr;
};

static struct dect_nrf91_mac_dev_context dect_nrf91_mac_dev_context_data;

/**************************************************************************************************/
/* Sanity checks between Zephyr and nrf api */
BUILD_ASSERT((NRF_MODEM_DECT_MAC_MAX_CHANNELS_IN_RSSI_SCAN == DECT_MAC_MAX_CHANNELS_IN_RSSI_SCAN),
	     "NRF_MODEM_DECT_MAC_MAX_CHANNELS_IN_RSSI_SCAN != "
	     "DECT_MAC_MAX_CHANNELS_IN_RSSI_SCAN");
BUILD_ASSERT((NRF_MODEM_DECT_MAC_MAX_CHANNELS_IN_NETWORK_SCAN_REQ ==
	      DECT_MAC_MAX_CHANNELS_IN_NETWORK_SCAN_REQ),
	     "NRF_MODEM_DECT_MAC_MAX_CHANNELS_IN_NETWORK_SCAN_REQ != "
	     "DECT_MAC_MAX_CHANNELS_IN_NETWORK_SCAN_REQ");
BUILD_ASSERT((NRF_MODEM_DECT_MAC_MAX_ADDITIONAL_NW_BEACON_CHANNELS ==
	      DECT_MAC_MAX_ADDITIONAL_NW_BEACON_CHANNELS),
	     "NRF_MODEM_DECT_MAC_MAX_ADDITIONAL_NW_BEACON_CHANNELS != "
	     "DECT_MAC_MAX_ADDITIONAL_NW_BEACON_CHANNELS");

BUILD_ASSERT((NRF_MODEM_DECT_MAC_INTEGRITY_KEY_LENGTH == DECT_MAC_INTEGRITY_KEY_LENGTH),
	     "NRF_MODEM_DECT_MAC_INTEGRITY_KEY_LENGTH != "
	     "DECT_MAC_INTEGRITY_KEY_LENGTH");
BUILD_ASSERT((NRF_MODEM_DECT_MAC_CIPHER_KEY_LENGTH == DECT_MAC_CIPHER_KEY_LENGTH),
	     "NRF_MODEM_DECT_MAC_CIPHER_KEY_LENGTH != "
	     "DECT_MAC_CIPHER_KEY_LENGTH");
BUILD_ASSERT((NRF_MODEM_DECT_MAC_SECURITY_MODE_NONE ==
	      (enum nrf_modem_dect_mac_security_mode)DECT_MAC_SECURITY_MODE_NONE),
	     "NRF_MODEM_DECT_MAC_SECURITY_MODE_NONE != "
	     "DECT_MAC_SECURITY_MODE_NONE");
BUILD_ASSERT((NRF_MODEM_DECT_MAC_SECURITY_MODE_1 ==
	      (enum nrf_modem_dect_mac_security_mode)DECT_MAC_SECURITY_MODE_1),
	     "NRF_MODEM_DECT_MAC_SECURITY_MODE_1 != "
	     "DECT_MAC_SECURITY_MODE_1");

/* Sanity check between dect_association_release_cause and nrf_modem_dect_mac_release_cause*/
BUILD_ASSERT((DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION ==
	      (enum dect_association_release_cause)
		      NRF_MODEM_DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION),
	     "DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION != "
	     "NRF_MODEM_DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION");
BUILD_ASSERT((DECT_MAC_RELEASE_CAUSE_MOBILITY ==
	      (enum dect_association_release_cause)NRF_MODEM_DECT_MAC_RELEASE_CAUSE_MOBILITY),
	     "DECT_MAC_RELEASE_CAUSE_MOBILITY != "
	     "NRF_MODEM_DECT_MAC_RELEASE_CAUSE_MOBILITY");
BUILD_ASSERT(
	(DECT_MAC_RELEASE_CAUSE_LONG_INACTIVITY ==
	 (enum dect_association_release_cause)NRF_MODEM_DECT_MAC_RELEASE_CAUSE_LONG_INACTIVITY),
	"DECT_MAC_RELEASE_CAUSE_LONG_INACTIVITY != "
	"NRF_MODEM_DECT_MAC_RELEASE_CAUSE_LONG_INACTIVITY");
BUILD_ASSERT((DECT_MAC_RELEASE_CAUSE_INCOMPATIBLE_CONFIGURATION ==
	      (enum dect_association_release_cause)
		      NRF_MODEM_DECT_MAC_RELEASE_CAUSE_INCOMPATIBLE_CONFIGURATION),
	     "DECT_MAC_RELEASE_CAUSE_INCOMPATIBLE_CONFIGURATION != "
	     "NRF_MODEM_DECT_MAC_RELEASE_CAUSE_INCOMPATIBLE_CONFIGURATION");
BUILD_ASSERT((DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_HW_RESOURCES ==
	      (enum dect_association_release_cause)
		      NRF_MODEM_DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_HW_RESOURCES),
	     "DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_HW_RESOURCES != "
	     "NRF_MODEM_DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_HW_RESOURCES");
BUILD_ASSERT((DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_RADIO_RESOURCES ==
	      (enum dect_association_release_cause)
		      NRF_MODEM_DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_RADIO_RESOURCES),
	     "DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_RADIO_RESOURCES != "
	     "NRF_MODEM_DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_RADIO_RESOURCES");
BUILD_ASSERT(
	(DECT_MAC_RELEASE_CAUSE_BAD_RADIO_QUALITY ==
	 (enum dect_association_release_cause)NRF_MODEM_DECT_MAC_RELEASE_CAUSE_BAD_RADIO_QUALITY),
	"DECT_MAC_RELEASE_CAUSE_BAD_RADIO_QUALITY != "
	"NRF_MODEM_DECT_MAC_RELEASE_CAUSE_BAD_RADIO_QUALITY");
BUILD_ASSERT((DECT_MAC_RELEASE_CAUSE_SECURITY_ERROR ==
	      (enum dect_association_release_cause)NRF_MODEM_DECT_MAC_RELEASE_CAUSE_SECURITY_ERROR),
	     "DECT_MAC_RELEASE_CAUSE_SECURITY_ERROR != "
	     "NRF_MODEM_DECT_MAC_RELEASE_CAUSE_SECURITY_ERROR");
BUILD_ASSERT((DECT_MAC_RELEASE_CAUSE_OTHER_ERROR ==
	      (enum dect_association_release_cause)NRF_MODEM_DECT_MAC_RELEASE_CAUSE_OTHER_ERROR),
	     "DECT_MAC_RELEASE_CAUSE_OTHER_ERROR != "
	     "NRF_MODEM_DECT_MAC_RELEASE_CAUSE_OTHER_ERROR");
BUILD_ASSERT((DECT_MAC_RELEASE_CAUSE_OTHER_REASON ==
	      (enum dect_association_release_cause)NRF_MODEM_DECT_MAC_RELEASE_CAUSE_OTHER_REASON),
	     "DECT_MAC_RELEASE_CAUSE_OTHER_REASON != "
	     "NRF_MODEM_DECT_MAC_RELEASE_CAUSE_OTHER_REASON");
BUILD_ASSERT((DECT_MAC_RELEASE_CAUSE_CUSTOM_RACH_RESOURCE_FAILURE ==
	      (enum dect_association_release_cause)DECT_MAC_RELEASE_CAUSE_RACH_RESOURCE_FAILURE),
	     "DECT_MAC_RELEASE_CAUSE_CUSTOM_RACH_RESOURCE_FAILURE != "
	     "DECT_MAC_RELEASE_CAUSE_RACH_RESOURCE_FAILURE");

/* Sanity check for the 1st and for the last error cause values */
BUILD_ASSERT((DECT_MAC_STATUS_OK == (enum dect_status_values)NRF_MODEM_DECT_MAC_STATUS_OK),
	     "NRF_MODEM_DECT_MAC_STATUS_OK != DECT_MAC_STATUS_OK");
BUILD_ASSERT((DECT_MAC_STATUS_NO_RSSI_RESULTS ==
	      (enum dect_status_values)NRF_MODEM_DECT_MAC_STATUS_NO_RSSI_RESULTS),
	     "DECT_MAC_STATUS_NO_RSSI_RESULTS != NRF_MODEM_DECT_MAC_STATUS_NO_RSSI_RESULTS");

/**************************************************************************************************/

static uint8_t *dect_nrf91_initial_link_addr_get(struct dect_nrf91_mac_dev_context *ctx)
{
	/* MAC addr based on set long rd id */
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	memset(ctx->link_addr, 0, sizeof(ctx->link_addr));

	if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_FT) {
		ctx->link_addr[0] =
			(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 24) & 0xFF;
		ctx->link_addr[1] =
			(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 16) & 0xFF;
		ctx->link_addr[2] =
			(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 8) & 0xFF;
		ctx->link_addr[3] =
			set_ptr->net_mgmt_common.identities.transmitter_long_rd_id & 0xFF;
		ctx->link_addr[4] =
			(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 24) & 0xFF;
		ctx->link_addr[5] =
			(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 16) & 0xFF;
		ctx->link_addr[6] =
			(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 8) & 0xFF;
		ctx->link_addr[7] =
			set_ptr->net_mgmt_common.identities.transmitter_long_rd_id & 0xFF;
	} else {
		if (ctx->parent_long_rd_id == 0) {
			/* Sink part to be set later when associated */
			ctx->link_addr[4] =
				(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 24) &
				0xFF;
			ctx->link_addr[5] =
				(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 16) &
				0xFF;
			ctx->link_addr[6] =
				(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 8) &
				0xFF;
			ctx->link_addr[7] =
				set_ptr->net_mgmt_common.identities.transmitter_long_rd_id & 0xFF;
		} else {
			ctx->link_addr[0] = (ctx->parent_long_rd_id >> 24) & 0xFF;
			ctx->link_addr[1] = (ctx->parent_long_rd_id >> 16) & 0xFF;
			ctx->link_addr[2] = (ctx->parent_long_rd_id >> 8) & 0xFF;
			ctx->link_addr[3] = ctx->parent_long_rd_id & 0xFF;
			ctx->link_addr[4] =
				(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 24) &
				0xFF;
			ctx->link_addr[5] =
				(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 16) &
				0xFF;
			ctx->link_addr[6] =
				(set_ptr->net_mgmt_common.identities.transmitter_long_rd_id >> 8) &
				0xFF;
			ctx->link_addr[7] =
				set_ptr->net_mgmt_common.identities.transmitter_long_rd_id & 0xFF;
		}
	}
	return ctx->link_addr;
}

/**************************************************************************************************/

static bool
dect_nrf91_child_association_list_nbr_add(struct dect_nrf91_association_data *ass_list_item)
{
	struct in6_addr child_addr = {};
	struct in6_addr prefix = {};
	struct in6_addr prefix_local = {};
	struct dect_nrf91_ipv6_prefix sink_global_prefix;
	bool child_addr_generated = false;
	bool added = false; /* Either local and/or global added */
	bool add_also_global = false;
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
	struct net_if *iface = dect_nrf91_mac_dev_context_data.iface;

	if (ass_list_item == NULL) {
		LOG_ERR("%s: no association data", (__func__));
		return false;
	}
	/* Add child as a neigbor, both local and global */
	if (dect_nrf91_sink_ipv6_prefix_get(&sink_global_prefix)) {
		memcpy(&prefix, sink_global_prefix.prefix.s6_addr, sink_global_prefix.len);

		add_also_global = true;
	}
	UNALIGNED_PUT(htonl(0xfe800000), &prefix_local.s6_addr32[0]);

	/* At 1st, add local nbr */
	child_addr_generated = dect_nrp_utils_net_ipv6_addr_create_from_sink_and_long_rd_id(
		prefix_local, set_ptr->net_mgmt_common.identities.transmitter_long_rd_id,
		ass_list_item->target_long_rd_id, &child_addr);
	if (child_addr_generated) {
		/* local: add a child as a eigbor to dect iface */
		if (!net_ipv6_nbr_add(iface, &child_addr, net_if_get_link_addr(iface), false,
				      NET_IPV6_NBR_STATE_REACHABLE)) {
			LOG_WRN("(%s): local: cannot add child (long rd id %d) as a nbr "
				"to dect iface",
				(__func__), ass_list_item->target_long_rd_id);
		} else {
			added = true;
			ass_list_item->local_ipv6_addr = child_addr;
			LOG_INF("(%s): child (long rd id %d) local addr %s (link addr %s) added "
				"as a neigbor to dect iface",
				(__func__), ass_list_item->target_long_rd_id,
				net_sprint_ipv6_addr(&ass_list_item->local_ipv6_addr),
				net_sprint_ll_addr(net_if_get_link_addr(iface)->addr, 8));
		}
	}
	if (add_also_global) {
		child_addr_generated = dect_nrp_utils_net_ipv6_addr_create_from_sink_and_long_rd_id(
			prefix, set_ptr->net_mgmt_common.identities.transmitter_long_rd_id,
			ass_list_item->target_long_rd_id, &child_addr);
		if (child_addr_generated) {
			/* global: add a child as a neigbor to dect iface */
			if (!net_ipv6_nbr_add(iface, &child_addr, net_if_get_link_addr(iface),
					      false, NET_IPV6_NBR_STATE_REACHABLE)) {
				LOG_WRN("(%s): global: Cannot add child (long rd id %d) as a nbr "
					"to dect iface",
					(__func__), ass_list_item->target_long_rd_id);
			} else {
				added = true;
				ass_list_item->global_ipv6_addr = child_addr;
				ass_list_item->global_ipv6_addr_set = true;
				LOG_INF("(%s): child (long rd id %d) global addr %s (link addr %s) "
					"added as a neigbor to dect iface",
					(__func__), ass_list_item->target_long_rd_id,
					net_sprint_ipv6_addr(&ass_list_item->global_ipv6_addr),
					net_sprint_ll_addr(net_if_get_link_addr(iface)->addr, 8));
			}
		}
	}

	return added;
}

static struct dect_nrf91_association_data *
dect_nrf91_child_association_list_item_get(uint32_t target_long_rd_id)
{
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;

	LOG_DBG("%s: target_long_rd_id %u", (__func__), target_long_rd_id);

	for (int i = 0; i < ARRAY_SIZE(ctx->child_associations); i++) {
		if (ctx->child_associations[i].in_use &&
		    ctx->child_associations[i].target_long_rd_id == target_long_rd_id) {
			return &ctx->child_associations[i];
		}
	}
	return NULL;
}

static struct dect_nrf91_association_data *
dect_nrf91_child_association_list_add(uint32_t target_long_rd_id)
{
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;
	struct dect_nrf91_association_data *ass_list_item = NULL;

	/* Return existing item if already in list */
	ass_list_item = dect_nrf91_child_association_list_item_get(target_long_rd_id);
	if (ass_list_item != NULL) {
		return ass_list_item;
	}

	/* Add to list */
	for (int i = 0; i < ARRAY_SIZE(ctx->child_associations); i++) {
		if (!ctx->child_associations[i].in_use) {
			ctx->child_associations[i].in_use = true;
			ctx->child_associations[i].target_long_rd_id = target_long_rd_id;
			return &ctx->child_associations[i];
		}
	}
	return NULL;
}

static void dect_nrf91_child_association_list_remove(uint32_t target_long_rd_id)
{
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;

	LOG_DBG("%s: target_long_rd_id %u", (__func__), target_long_rd_id);

	for (int i = 0; i < ARRAY_SIZE(ctx->child_associations); i++) {
		if (ctx->child_associations[i].in_use &&
		    ctx->child_associations[i].target_long_rd_id == target_long_rd_id) {
			ctx->child_associations[i].in_use = false;
			break;
		}
	}
}

/**************************************************************************************************/

static void dect_nrf91_parent_association_list_addressing_handle(
	struct dect_nrf91_association_data *ass_list_item,
	struct nrf_modem_dect_mac_ipv6_address_config_t ipv6_config_from_mdm)
{
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;
	struct net_if *iface = dect_nrf91_mac_dev_context_data.iface;
	struct net_if_addr *ifaddr;
	struct in6_addr iid;
	uint8_t *link_addr;
	int ret;

	/* Parent added: remove/update our link addr and ipv6 IID */
	dect_nrp_utils_net_ipv6_addr_create_iid(&iid, net_if_get_link_addr(iface));
	net_if_ipv6_addr_rm(iface, &iid);

	link_addr = dect_nrf91_initial_link_addr_get(ctx);
	ret = net_if_set_link_addr(iface, link_addr, 8, NET_LINK_UNKNOWN);
	if (ret) {
		LOG_WRN("%s: cannot re-set link addr: %d", (__func__), ret);
	}

	dect_nrp_utils_net_ipv6_addr_create_iid(&iid, net_if_get_link_addr(iface));
	ifaddr = net_if_ipv6_addr_add(iface, &iid, NET_ADDR_AUTOCONF, 0);
	if (!ifaddr) {
		LOG_WRN("%s: cannot add link address to interface %p", (__func__), iface);
	}
	struct in6_addr global_addr = {};
	struct in6_addr prefix_local = {};
	struct in6_addr parent_addr = {};
	bool parent_addr_generated = false;
	bool add_global = false;

	UNALIGNED_PUT(htonl(0xfe800000), &prefix_local.s6_addr32[0]);

	/* Set ipv6 addr based on given info from peer FT device */
	if (ipv6_config_from_mdm.type == NRF_MODEM_DECT_MAC_IPV6_ADDRESS_TYPE_NONE) {
		LOG_INF("No IPv6 address to set - using link local only");
	} else {
		int len = (ipv6_config_from_mdm.type == NRF_MODEM_DECT_MAC_IPV6_ADDRESS_TYPE_FULL)
				  ? 16
				  : 8;

		/* Create our own IPv6 address using the given prefix and iid. We first
		 * setup link local address, and then copy prefix over first 16/8
		 * bytes of that address.
		 */
		dect_nrp_utils_net_ipv6_addr_create_iid(&global_addr, net_if_get_link_addr(iface));
		memcpy(&global_addr, ipv6_config_from_mdm.address, len);

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
				ctx->global_ipv6_addr = global_addr;
				ctx->global_ipv6_addr_set = true;

				add_global = true;
				LOG_INF("Global IPv6 address %s added to interface %p",
					net_sprint_ipv6_addr(&global_addr), iface);
			}
		}
	}

	/* Add local addr as a neigbor and a router */
	parent_addr_generated = dect_nrp_utils_net_ipv6_addr_create_from_sink_and_long_rd_id(
		prefix_local, ass_list_item->target_long_rd_id, ass_list_item->target_long_rd_id,
		&parent_addr);
	if (parent_addr_generated) {
		/* local: add a parent as a neigbor to dect iface */
		if (!net_ipv6_nbr_add(iface, &parent_addr, net_if_get_link_addr(iface), false,
				      NET_IPV6_NBR_STATE_REACHABLE)) {
			LOG_ERR("(%s): local: cannot add child as a nbr to dect iface", (__func__));
		} else {
			ass_list_item->local_ipv6_addr = parent_addr;
			LOG_INF("(%s): local addr %s (link addr %s) added as a neigbor to dect "
				"iface",
				(__func__), net_sprint_ipv6_addr(&ass_list_item->local_ipv6_addr),
				net_sprint_ll_addr(net_if_get_link_addr(iface)->addr, 8));
		}
	}
	if (add_global) {
		parent_addr_generated =
			dect_nrp_utils_net_ipv6_addr_create_from_sink_and_long_rd_id(
				global_addr, ass_list_item->target_long_rd_id,
				ass_list_item->target_long_rd_id, &parent_addr);
		if (parent_addr_generated) {
			/* global: add a parent as a neigbor to dect iface */
			if (!net_ipv6_nbr_add(iface, &parent_addr, net_if_get_link_addr(iface),
					      true, NET_IPV6_NBR_STATE_REACHABLE)) {
				LOG_ERR("(%s): local: cannot add parent as a nbr to dect iface",
					(__func__));
			} else {
				LOG_INF("(%s): parent global addr %s (link addr %s) added as "
					"a neigbor to dect iface",
					(__func__),
					net_sprint_ipv6_addr(&ass_list_item->local_ipv6_addr),
					net_sprint_ll_addr(net_if_get_link_addr(iface)->addr, 8));
			}
			ass_list_item->global_ipv6_addr = parent_addr;
			ass_list_item->global_ipv6_addr_set = true;
		}
	}
}
/**************************************************************************************************/

static struct dect_nrf91_association_data *
dect_nrf91_parent_association_list_add(uint32_t target_long_rd_id)
{
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;

	LOG_DBG("%s: target_long_rd_id %u", (__func__), target_long_rd_id);
	for (int i = 0; i < ARRAY_SIZE(ctx->parent_associations); i++) {
		if (!ctx->parent_associations[i].in_use) {
			ctx->parent_associations[i].in_use = true;
			ctx->parent_associations[i].target_long_rd_id = target_long_rd_id;

			return &ctx->parent_associations[i];
		}
	}
	return NULL;
}

static struct dect_nrf91_association_data *
dect_nrf91_parent_association_list_item_get(uint32_t target_long_rd_id)
{
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;

	LOG_DBG("%s: target_long_rd_id %u", (__func__), target_long_rd_id);
	for (int i = 0; i < ARRAY_SIZE(ctx->parent_associations); i++) {
		if (ctx->parent_associations[i].in_use &&
		    ctx->parent_associations[i].target_long_rd_id == target_long_rd_id) {
			return &ctx->parent_associations[i];
		}
	}
	return NULL;
}

static void dect_nrf91_parent_association_list_remove(uint32_t target_long_rd_id)
{
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;

	LOG_DBG("dect_nrf91_parent_association_remove: target_long_rd_id %u", target_long_rd_id);
	for (int i = 0; i < ARRAY_SIZE(ctx->parent_associations); i++) {
		if (ctx->parent_associations[i].in_use &&
		    ctx->parent_associations[i].target_long_rd_id == target_long_rd_id) {
			ctx->parent_associations[i].in_use = false;
			ctx->parent_associations[i].target_long_rd_id = 0;
			break;
		}
	}
}

/**************************************************************************************************/

static struct net_mgmt_event_callback dect_nrf91_net_mgmt_ipv6_event_cb;

static void dect_nrf91_net_mgmt_ipv6_event_handler(struct net_mgmt_event_callback *cb,
						   uint32_t mgmt_event, struct net_if *iface)
{
	char ipv6_addr_str[NET_IPV6_ADDR_LEN];

	if (mgmt_event == NET_EVENT_IPV6_PREFIX_ADD) {
		struct net_event_ipv6_prefix *ipv6_prefix =
			(struct net_event_ipv6_prefix *)cb->info;

		LOG_WRN("NET_EVENT_IPV6_PREFIX_ADD: iface %p, prefix %s/%d", iface,
			net_addr_ntop(AF_INET6, (struct in6_addr *)&ipv6_prefix->addr,
				      ipv6_addr_str, NET_IPV6_ADDR_LEN),
			ipv6_prefix->len);

	} else if (mgmt_event == NET_EVENT_IPV6_PREFIX_DEL) {
		struct net_event_ipv6_prefix *ipv6_prefix =
			(struct net_event_ipv6_prefix *)cb->info;

		LOG_WRN("NET_EVENT_IPV6_PREFIX_DEL: iface %p, prefix %s/%d", iface,
			net_addr_ntop(AF_INET6, (struct in6_addr *)&ipv6_prefix->addr,
				      ipv6_addr_str, NET_IPV6_ADDR_LEN),
			ipv6_prefix->len);
	}
}

/**************************************************************************************************/

static void dect_nrf91_iface_init(struct net_if *iface)
{
	struct dect_nrf91_mac_dev_context *ctx = net_if_get_device(iface)->data;
	uint8_t *link_addr;
	int err;

	LOG_DBG("dect_nrf91_iface_init");

	net_mgmt_init_event_callback(&dect_nrf91_net_mgmt_ipv6_event_cb,
				     dect_nrf91_net_mgmt_ipv6_event_handler,
				     (NET_EVENT_IPV6_PREFIX_ADD | NET_EVENT_IPV6_PREFIX_DEL));
	net_mgmt_add_event_callback(&dect_nrf91_net_mgmt_ipv6_event_cb);

	/* Note: settings init takes for a while */
	dect_nrf91_settings_init();
	ctx->parent_long_rd_id = 0;
	link_addr = dect_nrf91_initial_link_addr_get(ctx);

	ctx->iface = iface;

	dect_nrf91_ctrl_init(iface);

	err = net_if_set_link_addr(iface, link_addr, 8, NET_LINK_UNKNOWN);
	if (err) {
		LOG_ERR("Cannot set link addr: %d", err);
		return;
	}
	err = net_if_set_name(ctx->iface, "nrf91_dect");
	if (err) {
		LOG_WRN("Could not set interface name!!");
	}

	struct net_if_addr *ifaddr;
	struct in6_addr iid;

	dect_nrp_utils_net_ipv6_addr_create_iid(&iid, net_if_get_link_addr(iface));

	ifaddr = net_if_ipv6_addr_add(iface, &iid, NET_ADDR_AUTOCONF, 0);
	if (!ifaddr) {
		LOG_ERR("Cannot add address to interface %p", iface);
	} else {
		/* As DAD is disabled in dect net iface,
		 * we need to mark the address as a preferred one.
		 */
		ifaddr->addr_state = NET_ADDR_PREFERRED;
	}

	if (IS_ENABLED(CONFIG_DECT_NRP_MAC_NET_IF_NO_AUTO_START)) {
		net_if_flag_set(iface, NET_IF_NO_AUTO_START);
	}
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	dect_net_l2_init(iface, &set_ptr->net_mgmt_common);
}

static int dect_nrf91_init(const struct device *dev)
{
	printk("nRF91 dect NR+ initialized\n");
	return 0;
}

/**************************************************************************************************/

static int dect_nrf91_ctrl_activate_cmd(const struct device *dev)
{
	return dect_nrf91_ctrl_configure_n_activate();
}

static int dect_nrf91_ctrl_deactivate_cmd(const struct device *dev)
{
	return dect_nrf91_ctrl_deactivate();
}

static int dect_nrf91_driver_rssi_scan(const struct device *dev,
				       struct dect_rssi_scan_params *params)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
	struct nrf_modem_dect_mac_rssi_scan_params scan_params = {
		.channel_scan_length = params->frame_count_to_scan,
		.num_channels = params->channel_count,
		.band = params->band,
		.threshold_min = set_ptr->net_mgmt_common.rssi_scan.free_threshold_dbm,
		.threshold_max = set_ptr->net_mgmt_common.rssi_scan.busy_threshold_dbm,
	};
	int err;

	if (params->band != 0) {
		scan_params.num_channels = 0;
	}
	for (int i = 0; i < params->channel_count; i++) {
		scan_params.channel_list[i] = params->channel_list[i];
	}

	err = dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_RSSI_START_REQ_CMD,
					       &scan_params,
					       sizeof(struct nrf_modem_dect_mac_rssi_scan_params));
	if (err) {
		LOG_ERR("%s: error initiating in RSSI scan: err %d", (__func__), err);
	} else {
		LOG_INF("%s: RSSI scan initiated", (__func__));
	}

	return err;
}

/**************************************************************************************************/

static int dect_nrf91_driver_scan(const struct device *dev, struct dect_scan_params *params,
				  dect_scan_result_cb_t cb)
{
	struct nrf_modem_dect_mac_network_scan_params scan_params = {
		.scan_time = params->channel_scan_time_ms,
		.network_id_filter_mode = NRF_MODEM_DECT_MAC_NW_ID_FILTER_MODE_NONE,
		.num_channels = params->channel_count,
		.band = params->band,
	};
	int err;

	__ASSERT_NO_MSG(cb != NULL);
	if (params->channel_count > NRF_MODEM_DECT_MAC_MAX_CHANNELS_IN_NETWORK_SCAN_REQ) {
		LOG_ERR("Channel count %d exceeds maximum %d", params->channel_count,
			NRF_MODEM_DECT_MAC_MAX_CHANNELS_IN_NETWORK_SCAN_REQ);
		return -EINVAL;
	}

	if (params->channel_scan_time_ms < 1 || params->channel_scan_time_ms > 60000) {
		LOG_ERR("Channel scan time %d is out of range (1-60000 ms)",
			params->channel_scan_time_ms);
		return -EINVAL;
	}

	if (params->band != 0) {
		scan_params.num_channels = 0;
	}

	for (int i = 0; i < params->channel_count; i++) {
		scan_params.channel_list[i] = params->channel_list[i];
	}

	err = dect_nrf91_ctrl_nw_scan_cmd(&scan_params, dect_nrf91_mac_dev_context_data.iface, cb);
	if (err) {
		LOG_ERR("Error initiating in Network scan: err %d", err);
	} else {
		LOG_INF("Network scan initiated");
	}

	return 0;
}

/**************************************************************************************************/

int dect_nrf91_driver_associate_req(const struct device *dev,
				    struct dect_associate_req_params *params)
{
	int ret;
	struct nrf_modem_dect_mac_association_params mdm_params;
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
	struct nrf_modem_dect_mac_tx_flow_config flow_config[6];

	/* Association req only with PT devices,
	 * TODO: should it be accepted also with FT? mdm seems to accept also when beacon running
	 */
	if (set_ptr->net_mgmt_common.device_type != DECT_DEVICE_TYPE_PT) {
		LOG_ERR("%s: Association request only supported for PT devices", (__func__));
		return -EINVAL;
	}

	mdm_params.long_rd_id = params->target_long_rd_id;
	mdm_params.network_id = set_ptr->net_mgmt_common.identities.network_id;
	mdm_params.num_flows = 1;

	mdm_params.info_triggers.num_beacon_rx_failures = 2;

	/* TODO: from settings or from command params? */
	flow_config[0].flow_id = 1;
	flow_config[0].priority = 1;
	flow_config[0].dlc_service_type = NRF_MODEM_DECT_DLC_SERVICE_TYPE_3;
	flow_config[0].num_arq_retx = 2;
	flow_config[0].dlc_sdu_lifetime = 255;

	mdm_params.tx_flow_configs = flow_config;

	ret = dect_nrf91_ctrl_associate_req_cmd(&mdm_params);
	if (ret) {
		LOG_ERR("%s: error in Associate: err %d", (__func__), ret);
	} else {
		LOG_INF("%s: association initiated", (__func__));
	}
	return ret;
}

int dect_nrf91_driver_associate_release(const struct device *dev,
					struct dect_associate_rel_params *params)
{
	int ret;

	ret = dect_nrf91_ctrl_associate_release_cmd(
		params->target_long_rd_id, NRF_MODEM_DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION);
	if (ret) {
		LOG_ERR("%s: error in Association Release: err %d", (__func__), ret);
	} else {
		LOG_INF("%s: association release initiated", (__func__));
	}
	return ret;
}

/**************************************************************************************************/

int dect_nrf91_driver_cluster_start_req(const struct device *dev,
					struct dect_cluster_start_req_params *params)
{
	return dect_nrf91_ctrl_cluster_start_req_cmd(params);
}

int dect_nrf91_driver_cluster_reconfig_req(const struct device *dev,
					   struct dect_cluster_reconfig_req_params *params)
{
	return dect_nrf91_ctrl_cluster_reconfig_req_cmd(params);
}

/**************************************************************************************************/

int dect_nrf91_driver_nw_beacon_start_req(const struct device *dev,
					  struct dect_nw_beacon_start_req_params *params)
{
	return dect_nrf91_ctrl_nw_beacon_start_req_cmd(params);
}

int dect_nrf91_driver_nw_beacon_stop_req(const struct device *dev,
					 struct dect_nw_beacon_stop_req_params *params)
{
	return dect_nrf91_ctrl_nw_beacon_stop_req_cmd(params);
}

/**************************************************************************************************/

static int dect_nrf91_driver_status_info_get(const struct device *dev,
					     struct dect_status_info *status_info_out)
{
	struct dect_status_info status_info = {};
	int i, tmp_count;
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	if (status_info_out == NULL) {
		LOG_ERR("%s: no status info pointer", (__func__));
		return -EINVAL;
	}
	status_info.mdm_activated = dect_nrf91_ctrl_mdm_activated();

	tmp_count = 0;

	/* Fill children status*/
	/* TODO: get ip addresses from neighbor table or remove totally from dect status,
	 * as aren't "dect status"?
	 */
	for (i = 0; i < ARRAY_SIZE(ctx->child_associations); i++) {
		if (ctx->child_associations[i].in_use) {
			status_info.child_associations[i].long_rd_id =
				ctx->child_associations[i].target_long_rd_id;
			status_info.child_associations[i].local_ipv6_addr =
				ctx->child_associations[i].local_ipv6_addr;
			status_info.child_associations[i].global_ipv6_addr =
				ctx->child_associations[i].global_ipv6_addr;
			status_info.child_associations[i].global_ipv6_addr_set =
				ctx->child_associations[i].global_ipv6_addr_set;
			tmp_count++;
		}
	}
	status_info.child_count = tmp_count;
	tmp_count = 0;

	/* Fill parent info status*/
	for (i = 0; i < ARRAY_SIZE(ctx->parent_associations); i++) {
		if (ctx->parent_associations[i].in_use) {
			status_info.parent_associations[i].long_rd_id =
				ctx->parent_associations[i].target_long_rd_id;
			status_info.parent_associations[i].local_ipv6_addr =
				ctx->parent_associations[i].local_ipv6_addr;
			status_info.parent_associations[i].global_ipv6_addr =
				ctx->parent_associations[i].global_ipv6_addr;
			status_info.parent_associations[i].global_ipv6_addr_set =
				ctx->parent_associations[i].global_ipv6_addr_set;
			tmp_count++;
		}
	}
	status_info.parent_count = tmp_count;

	status_info.cluster_channel = 0;
	status_info.cluster_running = false;

	/* TODO: Get cluster info if FT device etc.  */
	if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_FT) {
		int cluster_channel = dect_nrf91_ctrl_cluster_channel_get();

		if (cluster_channel > 0) {
			status_info.cluster_channel = cluster_channel;
			status_info.cluster_running = true;
		}
	}
	status_info.nw_beacon_running = dect_nrf91_ctrl_nw_beacon_running();

	strcpy(status_info.fw_version_str, "Not available");
#if defined(CONFIG_MODEM_INFO)
	char info_str[MODEM_INFO_MAX_RESPONSE_SIZE + 1];
	int ret;

	ret = modem_info_string_get(MODEM_INFO_FW_VERSION, info_str, sizeof(info_str));
	if (ret >= 0 && strlen(info_str) < sizeof(status_info.fw_version_str)) {
		strcpy(status_info.fw_version_str, info_str);
	}
#endif
	status_info.br_net_iface = NULL;
	status_info.br_global_ipv6_addr_prefix_set = false;
	status_info.br_global_ipv6_addr_prefix_len = 0;
	status_info.br_global_ipv6_addr_prefix = (struct in6_addr) {
		.s6_addr = { 0 }
	};

#if defined(CONFIG_DECT_NRP_MAC_BORDER_ROUTER)
	struct dect_nrf91_ipv6_prefix br_global_prefix = {
		.iface = NULL,
	};

	if (dect_nrf91_sink_ipv6_prefix_get(&br_global_prefix)) {
		status_info.br_global_ipv6_addr_prefix_set = true;
		net_ipaddr_copy(&status_info.br_global_ipv6_addr_prefix,
				&br_global_prefix.prefix);
		status_info.br_global_ipv6_addr_prefix_len = br_global_prefix.len;
		status_info.br_net_iface = br_global_prefix.iface;
	} else {
		status_info.br_global_ipv6_addr_prefix_set = false;
		/* There might be iface still set: */
		status_info.br_net_iface = br_global_prefix.iface;
	}
#endif

	*status_info_out = status_info;

	return 0;
}

/**************************************************************************************************/

static int dect_nrf91_driver_settings_read(const struct device *dev,
					   struct dect_settings *settings_out)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	*settings_out = set_ptr->net_mgmt_common;

	return 0;
}

static int dect_nrf91_driver_settings_write(const struct device *dev,
					    const struct dect_settings *settings_in)
{
	struct dect_nrf91_settings_write_status ret_status;

	if (settings_in->cmd_params.reset_to_driver_defaults) {
		ret_status = dect_nrf91_settings_defaults_set();
	} else {
		struct dect_nrf91_settings settings = {
			.net_mgmt_common = *settings_in,
		};

		ret_status = dect_nrf91_settings_write(&settings);
	}
	if (ret_status.status == 0 && ret_status.reactivate) {
		if (dect_nrf91_ctrl_mdm_reactivate()) {
			LOG_INF("Couldn't reconfigure/activate modem to apply new settings - "
				"reactivate is needed");
		}
	}

	return ret_status.status;
}

/**************************************************************************************************/
static K_MUTEX_DEFINE(send_buf_lock); /* Only one TX requests at a time */

#define NRF91_DECT_UL_BUFFER_SIZE DECT_NRP_MTU

#if defined(CONFIG_DECT_NRP_MAC_NRF_TX_FLOW_CTRL_BASED_ON_MDM_TX_DLC_REQS)
/* We are using handle as array index */
BUILD_ASSERT(DECT_MAC_DATA_TX_HANDLE_COUNT == DECT_NRF91_DLC_DATA_INFO_MAX_COUNT,
	     "Mismatch in DECT MAC data tx handle range and DECT_NRF91_DLC_DATA_INFO_MAX_COUNT");
#endif
static int dect_nrf91_driver_send(const struct device *dev, struct net_pkt *pkt)
{
	__ASSERT_NO_MSG(pkt != NULL);

	dect_nrf91_ctrl_tx_cmd_params_t tx_params;
	uint32_t target_long_rd_id = 0;
	static uint32_t transaction_id = DECT_MAC_DATA_TX_HANDLE_START;
	int ret;
	int data_len = 0;

	if (IS_ENABLED(CONFIG_NET_IPV6) && net_pkt_family(pkt) == AF_INET6) {
		target_long_rd_id = dect_nrp_utils_dst_long_rd_id_get_from_pkt_dst_addr(pkt);

		if (!target_long_rd_id) {
			LOG_ERR("AF_INET6: No target long rd id in packet dst address");
			return -EINVAL;
		}
	} else if (IS_ENABLED(CONFIG_NET_SOCKETS_PACKET) && net_pkt_family(pkt) == AF_PACKET) {

		target_long_rd_id = dect_nrp_utils_dst_long_rd_id_get_from_pkt_dst_addr(pkt);

		if (!target_long_rd_id) {
			LOG_ERR("AF_PACKET: No target long rd id in packet dst address");
			return -EINVAL;
		}
	} else {
		LOG_ERR("%s: packet type %d not supported", (__func__), net_pkt_family(pkt));
		return -EINVAL;
	}

	LOG_DBG("dect_nrf91_driver_send: target_long_rd_id %u", target_long_rd_id);

	k_mutex_lock(&send_buf_lock, K_FOREVER);
	data_len = net_pkt_get_len(pkt);

	if (data_len > NRF91_DECT_UL_BUFFER_SIZE) {
		LOG_ERR("Packet too large: %d", data_len);
		k_mutex_unlock(&send_buf_lock);
		return -EINVAL;
	}

	ret = net_pkt_read(pkt, tx_params.data, data_len);
	if (ret < 0) {
		LOG_ERR("%s: cannot read packet: %d, from pkt %p, data_len %d\n", __func__, ret,
			pkt, data_len);
	} else {
		int retry_count = 0;
		bool data_sent = false;
		uint32_t sleep_ms = 50;

		tx_params.data_len = data_len;
		tx_params.long_rd_id = target_long_rd_id;
		tx_params.flow_id = 1;
		tx_params.transaction_id =
			transaction_id++; /* TODO transaction_id handling to ctrl side */

		while (retry_count < 20 &&
		       data_sent == false) { /* TODO: time (with Kconfig) instead of count */
			ret = dect_nrf91_ctrl_tx_cmd(&tx_params);
			if (ret == -ENOMEM || ret == -EACCES) {
				k_sleep(K_MSEC(sleep_ms));
				retry_count++;
			} else if (ret == -EBUSY) {
				tx_params.transaction_id = transaction_id++;
				retry_count++;
			} else {
				data_sent = true;
			}
			if (transaction_id > DECT_MAC_DATA_TX_HANDLE_END) {
				transaction_id = DECT_MAC_DATA_TX_HANDLE_START;
			}
		}

		if (data_sent && ret == 0) {
			LOG_DBG("Packet sending initiated to %d (%d bytes) after %d retries",
				target_long_rd_id, data_len, retry_count);
			if (retry_count) {
				LOG_DBG("Packet sending initiated to %d (%d bytes) "
					"after %d retries",
					target_long_rd_id, data_len, retry_count);
			}
			ret = 0;
		} else {
			LOG_ERR("Error sending packet: %d, retries %d", ret, retry_count);
		}
	}
	k_mutex_unlock(&send_buf_lock);
	/* If something went wrong, then we need to return negative value to
	 * net_if.c:net_if_tx() so that the net_pkt will get released.
	 */

	return ret;
}

static int dect_nrf91_driver_neighbor_list_req(const struct device *dev)
{
	return dect_nrf91_ctrl_neighbor_list_req_cmd();
}

static int dect_nrf91_driver_neighbor_info_req(const struct device *dev,
					       struct dect_neighbor_info_req_params *params)
{
	struct nrf_modem_dect_mac_neighbor_info_params mdm_params = {
		.long_rd_id = params->long_rd_id,
	};

	return dect_nrf91_ctrl_neighbor_info_req_cmd(&mdm_params);
}

static int dect_nrf91_driver_cluster_info_req(const struct device *dev)
{
	return dect_nrf91_ctrl_cluster_info_req_cmd();
}

static int dect_nrf91_ctrl_network_create_req(const struct device *dev)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_FT) {
		return dect_nrf91_ctrl_network_create_req_cmd();
	} else {
		return -ENOTSUP;
	}
}

static int dect_nrf91_ctrl_network_remove_req(const struct device *dev)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;
	int i, ret;

	if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_FT) {
		if (dect_nrf91_ctrl_network_remove_req_cmd_allowed() == false) {
			LOG_ERR("%s: Network remove not allowed", (__func__));
			return -EPERM;
		}
		for (i = 0; i < ARRAY_SIZE(ctx->child_associations); i++) {
			if (ctx->child_associations[i].in_use) {
				/* We are shooting all without waiting an answer */
				ret = dect_nrf91_ctrl_associate_release_cmd(
					ctx->child_associations[i].target_long_rd_id,
					NRF_MODEM_DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION);
				if (ret) {
					LOG_ERR("%s: error in Association Release: err %d",
						(__func__), ret);
				} else {
					LOG_INF("%s: association release initiated", (__func__));
				}
			}
		}
		k_sleep(K_MSEC(1000)); /* Wait for a while that all association are released */
		return dect_nrf91_ctrl_network_remove_req_cmd();
	} else {
		return -ENOTSUP;
	}
}

static int dect_nrf91_ctrl_network_join_req(const struct device *dev)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_PT) {
		return dect_nrf91_ctrl_network_join_req_cmd();
	} else {
		return -ENOTSUP;
	}
}

static int dect_nrf91_ctrl_network_unjoin_req(const struct device *dev)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_PT) {
		return dect_nrf91_ctrl_network_unjoin_req_cmd();
	} else {
		return -ENOTSUP;
	}
}

/**************************************************************************************************/

static const struct dect_nrp_hal_api dect_nrf91_api = {
	.iface_api.init = dect_nrf91_iface_init,
	.activate_req = dect_nrf91_ctrl_activate_cmd,
	.deactivate_req = dect_nrf91_ctrl_deactivate_cmd,
	.send = dect_nrf91_driver_send,
	.rssi_scan = dect_nrf91_driver_rssi_scan,
	.scan = dect_nrf91_driver_scan,
	.associate_req = dect_nrf91_driver_associate_req,
	.associate_release = dect_nrf91_driver_associate_release,
	.cluster_start_req = dect_nrf91_driver_cluster_start_req,
	.cluster_reconfig_req = dect_nrf91_driver_cluster_reconfig_req,
	.nw_beacon_start_req = dect_nrf91_driver_nw_beacon_start_req,
	.nw_beacon_stop_req = dect_nrf91_driver_nw_beacon_stop_req,
	.settings_read = dect_nrf91_driver_settings_read,
	.settings_write = dect_nrf91_driver_settings_write,
	.status_info_get = dect_nrf91_driver_status_info_get,
	.neighbor_list_req = dect_nrf91_driver_neighbor_list_req,
	.neighbor_info_req = dect_nrf91_driver_neighbor_info_req,
	.cluster_info_req = dect_nrf91_driver_cluster_info_req,
	.network_create_req = dect_nrf91_ctrl_network_create_req,
	.network_remove_req = dect_nrf91_ctrl_network_remove_req,
	.network_join_req = dect_nrf91_ctrl_network_join_req,
	.network_unjoin_req = dect_nrf91_ctrl_network_unjoin_req,
};

NET_DEVICE_INIT(nrf91_dect_mac_driver, "nrf91_dect_mac_driver", dect_nrf91_init, NULL,
		&dect_nrf91_mac_dev_context_data, NULL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		&dect_nrf91_api, DECT_L2, NET_L2_GET_CTX_TYPE(DECT_L2), DECT_NRP_MTU);

#if defined(CONFIG_NET_CONNECTION_MANAGER)
#include "dect_net_conn_mgr.h"
CONNECTIVITY_DECT_MGMT_BIND(nrf91_dect_mac_driver);
#endif

/**************************************************************************************************/

void dect_nrf91_parent_association_created(
	uint32_t target_long_rd_id, struct nrf_modem_dect_mac_ipv6_address_config_t ipv6_config)
{
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;
	struct net_if *iface = dect_nrf91_mac_dev_context_data.iface;
	struct dect_nrf91_association_data *ass_data_item;
	struct dect_network_status_evt network_status_data = {
		.network_status = DECT_NETWORK_STATUS_JOINED,
	};

	ass_data_item = dect_nrf91_parent_association_list_add(target_long_rd_id);
	if (ass_data_item == NULL) {
		LOG_ERR("Cannot add parent to association list - releasing association");
		goto association_release;
	}

	__ASSERT_NO_MSG(ctx->parent_long_rd_id == 0); /* Only one parent supported */
	ctx->parent_long_rd_id = target_long_rd_id;

	dect_nrf91_parent_association_list_addressing_handle(ass_data_item, ipv6_config);

	dect_net_l2_parent_association_created(iface, target_long_rd_id);

	/* We send also network status event eventhough join hasn't been necessarily called */
	dect_mgmt_network_status_evt(iface, network_status_data);

	return;

association_release:
	dect_nrf91_parent_association_list_remove(target_long_rd_id);
	dect_nrf91_ctrl_associate_release_cmd(
		target_long_rd_id, NRF_MODEM_DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_HW_RESOURCES);
}

void dect_nrf91_child_association_created(uint32_t target_long_rd_id)
{
	struct dect_nrf91_association_data *association_list_item;
	struct net_if *iface = dect_nrf91_mac_dev_context_data.iface;

	association_list_item = dect_nrf91_child_association_list_add(target_long_rd_id);
	if (association_list_item == NULL) {
		LOG_ERR("Cannot add child (long rd id %d) to association list - "
			"releasing association",
			association_list_item->target_long_rd_id);
		goto association_release;
	}

	/* Add child as a neigbor for a dect iface */
	if (!dect_nrf91_child_association_list_nbr_add(association_list_item)) {
		LOG_WRN("Cannot add child (long rd id %d) to neighbor list - continue",
			association_list_item->target_long_rd_id);
	}

	dect_net_l2_child_association_created(iface, target_long_rd_id);
	return;

association_release:
	dect_nrf91_child_association_list_remove(target_long_rd_id);
	dect_nrf91_ctrl_associate_release_cmd(
		target_long_rd_id,
		NRF_MODEM_DECT_MAC_RELEASE_CAUSE_INSUFFICIENT_HW_RESOURCES);
}

void dect_nrf91_parent_association_removed(
	uint32_t long_rd_id, enum nrf_modem_dect_mac_release_cause rel_cause)
{
	struct net_if *iface = dect_nrf91_mac_dev_context_data.iface;
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;

	dect_net_l2_association_removed(
		iface, long_rd_id, (enum dect_association_release_cause)rel_cause);

	/* We are orphan now. The only possible parent is gone. */
	struct net_if_addr *ifaddr;
	struct in6_addr iid;
	uint8_t *link_addr;
	int ret;

	/* We send also network status event eventhough unjoin hasn't been necessarily called */
	struct dect_network_status_evt network_status_data = {
		.network_status = DECT_NETWORK_STATUS_UNJOINED,
	};

	dect_mgmt_network_status_evt(iface, network_status_data);

	/* Parent removed: remove/update link addr and ipv6 IID */
	dect_nrp_utils_net_ipv6_addr_create_iid(&iid, net_if_get_link_addr(iface));
	net_if_ipv6_addr_rm(iface, &iid);

	__ASSERT_NO_MSG(dect_nrf91_mac_dev_context_data.parent_long_rd_id != 0);
	dect_nrf91_mac_dev_context_data.parent_long_rd_id = 0;
	link_addr = dect_nrf91_initial_link_addr_get(&dect_nrf91_mac_dev_context_data);

	ret = net_if_set_link_addr(iface, link_addr, 8, NET_LINK_UNKNOWN);
	if (ret) {
		LOG_ERR("%s: cannot set link addr: %d", (__func__), ret);
	}

	dect_nrp_utils_net_ipv6_addr_create_iid(&iid, net_if_get_link_addr(iface));
	ifaddr = net_if_ipv6_addr_add(iface, &iid, NET_ADDR_AUTOCONF, 0);
	if (!ifaddr) {
		LOG_ERR("%s: cannot add address to interface %p", (__func__), iface);
	}
	struct dect_nrf91_association_data *ass_list_item =
		dect_nrf91_parent_association_list_item_get(long_rd_id);

	if (!ass_list_item) {
		LOG_WRN("%s: no association with %d in a parent list", (__func__), long_rd_id);
		return;
	}

	/* Remove parent from neighbors */
	if (!net_ipv6_nbr_rm(iface, &ass_list_item->local_ipv6_addr)) {
		LOG_WRN("Cannot remove parent (long rd id %d, local addr %s) as a nbr "
			"from dect iface",
			ass_list_item->target_long_rd_id,
			net_sprint_ipv6_addr(&ass_list_item->local_ipv6_addr));
	}
	if (ctx->global_ipv6_addr_set &&
	    !net_ipv6_nbr_rm(iface, &ass_list_item->global_ipv6_addr)) {
		LOG_WRN("Cannot remove parent (long rd id %d, global addr %s) as a nbr "
			"from dect iface",
			ass_list_item->target_long_rd_id,
			net_sprint_ipv6_addr(&ass_list_item->global_ipv6_addr));
	}

	/* Our local address was already updated, now remove our global IP address */
	if (ctx->global_ipv6_addr_set) {
		net_if_ipv6_addr_rm(iface, &ctx->global_ipv6_addr);
	}
	ctx->global_ipv6_addr_set = false;

	/* ...and finally, remove from our list */
	dect_nrf91_parent_association_list_remove(long_rd_id);
}

void dect_nrf91_child_association_removed(
	uint32_t long_rd_id, enum nrf_modem_dect_mac_release_cause rel_cause)
{
	struct net_if *iface = dect_nrf91_mac_dev_context_data.iface;
	struct dect_nrf91_association_data *ass_list_item =
		dect_nrf91_child_association_list_item_get(long_rd_id);

	dect_net_l2_association_removed(
		iface, long_rd_id, (enum dect_association_release_cause)rel_cause);

	if (ass_list_item) {
		if (!net_ipv6_nbr_rm(iface, &ass_list_item->local_ipv6_addr)) {
			LOG_DBG("Cannot remove child (long rd id %d, local addr %s) as a nbr from "
				"dect iface",
				ass_list_item->target_long_rd_id,
				net_sprint_ipv6_addr(&ass_list_item->local_ipv6_addr));
		}
		if (!net_ipv6_nbr_rm(iface, &ass_list_item->global_ipv6_addr)) {
			LOG_DBG("Cannot remove child (long rd id %d, global addr %s) as a nbr from "
				"dect iface",
				ass_list_item->target_long_rd_id,
				net_sprint_ipv6_addr(&ass_list_item->global_ipv6_addr));
		}
		dect_nrf91_child_association_list_remove(long_rd_id);
	}
}

void dect_nrf91_child_association_all_removed(void)
{
	struct dect_nrf91_mac_dev_context *ctx = &dect_nrf91_mac_dev_context_data;

	for (int i = 0; i < ARRAY_SIZE(ctx->child_associations); i++) {
		if (ctx->child_associations[i].in_use) {
			dect_net_l2_association_removed(
				ctx->iface,
				ctx->child_associations[i].target_long_rd_id,
				DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION);
			dect_nrf91_child_association_list_remove(
				ctx->child_associations[i].target_long_rd_id);
		}
	}
}
