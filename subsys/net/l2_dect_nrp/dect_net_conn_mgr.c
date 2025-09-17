/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/sys/printk.h> /* TODO LOGging*/

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/conn_mgr_connectivity_impl.h>
#include <dect_net_l2_mgmt.h>

static struct net_mgmt_event_callback dect_mgmt_cb;
static int64_t connection_timeout;

static int connect(struct net_if *iface)
{
	struct dect_settings current_settings;
	int ret;

	ret = net_mgmt(NET_REQUEST_DECT_SETTINGS_READ, iface, &current_settings,
		       sizeof(current_settings));
	if (ret) {
		printk("dect_nrp_net_if_connect: cannot read current settings: %d\n", ret);
		return ret;
	}

	if (current_settings.device_type == DECT_DEVICE_TYPE_PT) {
		ret = net_mgmt(NET_REQUEST_DECT_NETWORK_JOIN, iface, NULL, 0);
		if (ret) {
			printk("dect_nrp_net_if_connect: cannot initiate request for joining "
			       "a network: %d\n",
			       ret);
		}
	} else {
		__ASSERT_NO_MSG(current_settings.device_type == DECT_DEVICE_TYPE_FT);

		ret = net_mgmt(NET_REQUEST_DECT_NETWORK_CREATE, iface, NULL, 0);
		if (ret) {
			printk("dect_nrp_net_if_connect: cannot initiate request for creating "
			       "a network: %d\n",
			       ret);
		}
	}

	return ret;
}

int dect_nrp_net_if_connect(struct conn_mgr_conn_binding *const binding)
{
	if (binding->timeout) {
		connection_timeout = k_uptime_get() + binding->timeout * MSEC_PER_SEC;
	} else {
		connection_timeout = 0;
	}

	return connect(binding->iface);
}

int dect_nrp_net_if_disconnect(struct conn_mgr_conn_binding *const binding)
{
	struct dect_settings current_settings;
	int ret;

	ret = net_mgmt(NET_REQUEST_DECT_SETTINGS_READ, binding->iface, &current_settings,
		       sizeof(current_settings));
	if (ret) {
		printk("dect_nrp_net_if_disconnect: cannot read current settings: %d\n", ret);
		goto exit;
	}
	if (current_settings.device_type == DECT_DEVICE_TYPE_PT) {
		ret = net_mgmt(NET_REQUEST_DECT_NETWORK_UNJOIN, binding->iface, NULL, 0);
		if (ret) {
			printk("dect_nrp_net_if_disconnect: cannot initiate request for unjoining "
			       "a network: %d\n",
			       ret);
		}
	} else {
		__ASSERT_NO_MSG(current_settings.device_type == DECT_DEVICE_TYPE_FT);
		ret = net_mgmt(NET_REQUEST_DECT_NETWORK_REMOVE, binding->iface, NULL, 0);
		if (ret) {
			printk("dect_nrp_net_if_disconnect: cannot initiate request for removing "
			       "a network: %d\n",
			       ret);
		}
	}
exit:
	return ret;
}

static void dect_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_DECT_NETWORK_STATUS: {
		struct dect_network_status_evt *evt = (struct dect_network_status_evt *)cb->info;

		if (evt->network_status == DECT_NETWORK_STATUS_FAILURE &&
		    evt->dect_err_cause == DECT_MAC_STATUS_RD_NOT_FOUND) {
			/* TODO: handle all errors. */
			if (!connection_timeout || k_uptime_get() < connection_timeout) {
				connect(iface);
				printk("\nconnect\n");
			}
		}
		break;
	}
	case NET_EVENT_DECT_ASSOCIATION_CHANGED: {
		struct dect_association_changed_evt *evt =
			(struct dect_association_changed_evt *)cb->info;

		if (conn_mgr_if_get_flag(iface, CONN_MGR_IF_PERSISTENT) &&
		    evt->neighbor_role == DECT_NEIGHBOR_ROLE_PARENT &&
		    evt->association_change_type == DECT_ASSOCIATION_RELEASED &&
		    evt->association_released.release_cause !=
					DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION) {
			/* Lost connection to parent but not because on purpose so reconnect. */
			dect_nrp_net_if_connect(conn_mgr_if_get_binding(iface));
			printk("\nif_connect\n");
		}
		break;
	}
	default:
		break;
	}
}

void dect_nrp_net_if_init(struct conn_mgr_conn_binding *const binding)
{
	/* Configure the interface according to kconfig values. */
	if (!IS_ENABLED(CONFIG_NET_L2_DECT_MGMT_AUTO_CONNECT)) {
		conn_mgr_binding_set_flag(binding, CONN_MGR_IF_NO_AUTO_CONNECT, true);
	}

	if (!IS_ENABLED(CONFIG_NET_L2_DECT_MGMT_AUTO_DOWN)) {
		conn_mgr_binding_set_flag(binding, CONN_MGR_IF_NO_AUTO_DOWN, true);
	}

	if (IS_ENABLED(CONFIG_NET_L2_DECT_MGMT_CONNECTION_PERSISTENCE)) {
		conn_mgr_binding_set_flag(binding, CONN_MGR_IF_PERSISTENT, true);
	}

	if (CONFIG_NET_L2_DECT_MGMT_CONNECT_TIMEOUT_SECONDS > 0) {
		conn_mgr_if_set_timeout(binding->iface,
					CONFIG_NET_L2_DECT_MGMT_CONNECT_TIMEOUT_SECONDS);
	}

	net_mgmt_init_event_callback(&dect_mgmt_cb, dect_event_handler,
			NET_EVENT_DECT_NETWORK_STATUS | NET_EVENT_DECT_ASSOCIATION_CHANGED);
	net_mgmt_add_event_callback(&dect_mgmt_cb);
}

static struct conn_mgr_conn_api l2_dect_conn_api = {
	.connect = dect_nrp_net_if_connect,
	.disconnect = dect_nrp_net_if_disconnect,
	.init = dect_nrp_net_if_init,
};

CONN_MGR_CONN_DEFINE(CONNECTIVITY_DECT_MGMT, &l2_dect_conn_api);
