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

/* Store a local reference to the connection binding */
static struct net_if *dect_iface;

int dect_nrp_net_if_connect(struct conn_mgr_conn_binding *const binding)
{
	struct dect_settings current_settings;
	int ret;

	ret = net_mgmt(NET_REQUEST_DECT_SETTINGS_READ, dect_iface, &current_settings,
		       sizeof(current_settings));
	if (ret) {
		printk("dect_nrp_net_if_connect: cannot read current settings: %d\n", ret);
		goto exit;
	}
	if (current_settings.device_type == DECT_DEVICE_TYPE_PT) {
		ret = net_mgmt(NET_REQUEST_DECT_NETWORK_JOIN, dect_iface, NULL, 0);
		if (ret) {
			printk("dect_nrp_net_if_connect: cannot initiate request for joining "
			       "a network: %d\n",
			       ret);
		}
	} else {
		__ASSERT_NO_MSG(current_settings.device_type == DECT_DEVICE_TYPE_FT);

		ret = net_mgmt(NET_REQUEST_DECT_NETWORK_CREATE, dect_iface, NULL, 0);
		if (ret) {
			printk("dect_nrp_net_if_connect: cannot initiate request for creating "
			       "a network: %d\n",
			       ret);
		}
	}
exit:
	return ret;
}

int dect_nrp_net_if_disconnect(struct conn_mgr_conn_binding *const binding)
{
	ARG_UNUSED(binding);

	struct dect_settings current_settings;
	int ret;

	ret = net_mgmt(NET_REQUEST_DECT_SETTINGS_READ, dect_iface, &current_settings,
		       sizeof(current_settings));
	if (ret) {
		printk("dect_nrp_net_if_disconnect: cannot read current settings: %d\n", ret);
		goto exit;
	}
	if (current_settings.device_type == DECT_DEVICE_TYPE_PT) {
		ret = net_mgmt(NET_REQUEST_DECT_NETWORK_UNJOIN, dect_iface, NULL, 0);
		if (ret) {
			printk("dect_nrp_net_if_disconnect: cannot initiate request for unjoining "
			       "a network: %d\n",
			       ret);
		}
	} else {
		__ASSERT_NO_MSG(current_settings.device_type == DECT_DEVICE_TYPE_FT);
		ret = net_mgmt(NET_REQUEST_DECT_NETWORK_REMOVE, dect_iface, NULL, 0);
		if (ret) {
			printk("dect_nrp_net_if_disconnect: cannot initiate request for removing "
			       "a network: %d\n",
			       ret);
		}
	}
exit:
	return ret;
}

void dect_nrp_net_if_init(struct conn_mgr_conn_binding *const binding)
{
	/* TODO: set iface connectivity flags (persistency, auto-connect and auto-down). */

	/* No auto-connect support for now; otherwise, the connection manager might try activating
	 * the DECT NRP stack before the modem is initialized. To fix this, enabling L2 (done when
	 * bringing up the interface) should initialize the modem.
	 */
	conn_mgr_binding_set_flag(binding, CONN_MGR_IF_NO_AUTO_CONNECT, true);

	conn_mgr_binding_set_flag(binding, CONN_MGR_IF_NO_AUTO_DOWN, true);

	/* TODO: configure connection timeout. */

	/* TODO: subscribe to relevant net events; NET_EVENT_DECT_NETWORK_STATUS, ??? */

	/* Keep local reference to the network interface that the connectivity layer is bound to. */
	dect_iface = binding->iface;
}

static struct conn_mgr_conn_api l2_dect_conn_api = {
	.connect = dect_nrp_net_if_connect,
	.disconnect = dect_nrp_net_if_disconnect,
	.init = dect_nrp_net_if_init,
};

CONN_MGR_CONN_DEFINE(CONNECTIVITY_DECT_MGMT, &l2_dect_conn_api);
