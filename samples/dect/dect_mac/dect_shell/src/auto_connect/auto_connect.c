
/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>

#include <zephyr/kernel.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#if defined(CONFIG_NET_CONNECTION_MANAGER)
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#endif
#include <zephyr/settings/settings.h>

#include "dect_net_l2_mgmt.h"

#include "dect_l2_shell_util.h"
#include "desh_print.h"
#include "auto_connect.h"

extern struct k_work_q desh_common_work_q;

static struct net_mgmt_event_callback auto_connect_net_mgmt_cb;
static struct {
	struct net_if *iface;
	bool connected;
} context;

static bool sett_auto_connect_enabled;
static int sett_auto_connect_delay_secs;

K_SEM_DEFINE(auto_conn_sink_connected_sem, 0, 1);

#define AUTO_CONNECT_SETT_KEY	  "desh_auto_connect_settings"
#define AUTO_CONNECT_SETT_ENABLED "enabled"
#define AUTO_CONNECT_SETT_DELAY	  "delay_secs"

/**@brief Callback when settings_load() is called. */
static int auto_connect_sett_handler(const char *key, size_t len, settings_read_cb read_cb,
				     void *cb_arg)
{
	int ret;

	if (strcmp(key, AUTO_CONNECT_SETT_ENABLED) == 0) {
		ret = read_cb(cb_arg, &sett_auto_connect_enabled,
			      sizeof(sett_auto_connect_enabled));
		if (ret < 0) {
			desh_error("Failed to read sett_auto_connect_enabled, error: %d", ret);
			return ret;
		}
	} else if (strcmp(key, AUTO_CONNECT_SETT_DELAY) == 0) {
		ret = read_cb(cb_arg, &sett_auto_connect_delay_secs,
			      sizeof(sett_auto_connect_delay_secs));
		if (ret < 0) {
			desh_error("Failed to read auto connect delay, error: %d", ret);
			return ret;
		}
	}
	return 0;
}

static void auto_connect_work_fn(struct k_work *item)
{
	ARG_UNUSED(item);

	if (context.connected) {
		desh_warn("Already connected");
	}
#if defined(CONFIG_NET_CONNECTION_MANAGER)
	int ret;
#if defined(CONFIG_DECT_NRP_MAC_BORDER_ROUTER)
	int additional_secs_to_wait = 10;

	/* Wait still for a while if sink is getting connected,
	 * TODO: this could be in connmgr instead?
	 */
	if (auto_connect_sett_delay_get() == 0) {
		additional_secs_to_wait = 0;
	}
	ret = k_sem_take(&auto_conn_sink_connected_sem, K_SECONDS(additional_secs_to_wait));
	if (ret) {
		desh_warn("Sink is not yet connected, but continuing with auto connect");
	}
#endif
	ret = conn_mgr_if_connect(context.iface);
	if (ret < 0) {
		desh_error("Failed to initiate a connect: %d", ret);
		return;
	}
	desh_print("auto_connect: connect initiated.");
#else
	struct dect_settings current_settings;
	int err;

	err = net_mgmt(NET_REQUEST_DECT_SETTINGS_READ, context.iface, &current_settings,
		       sizeof(current_settings));
	if (err) {
		desh_error("Cannot read current settings: %d", err);
		return;
	}
	if (current_settings.device_type == DECT_DEVICE_TYPE_PT) {
		err = net_mgmt(NET_REQUEST_DECT_NETWORK_JOIN, context.iface, NULL, 0);
		if (err) {
			desh_error("auto_connect: cannot initiate request for joining "
				   "a network: %d",
				   err);
		} else {
			desh_print("auto_connect: network joining initiated.");
		}

	} else {
		__ASSERT_NO_MSG(current_settings.device_type == DECT_DEVICE_TYPE_FT);
		err = net_mgmt(NET_REQUEST_DECT_NETWORK_CREATE, context.iface, NULL, 0);
		if (err) {
			desh_error("auto_connect: cannot initiate request for creating "
				   "a network: %d",
				   err);
		} else {
			desh_print("auto_connect: network creation initiated.");
		}
	}
#endif
}

K_WORK_DELAYABLE_DEFINE(auto_connect_work, auto_connect_work_fn);

/**************************************************************************************************/
#if defined(CONFIG_NET_CONNECTION_MANAGER)
#define L4_EVENT_MASK                                                                              \
	(NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED | NET_EVENT_L4_IPV6_CONNECTED |        \
	 NET_EVENT_L4_IPV6_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

static void l4_event_handler(struct net_mgmt_event_callback *cb, uint32_t event,
			     struct net_if *iface)
{
	if (iface != context.iface) {
		desh_warn("Received L4 event for an unexpected interface (%p), expected %p", iface,
			  context.iface);
		return;
	}
	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		desh_print("NET_EVENT_L4_CONNECTED: Network connectivity established "
			   "and global IPv6 address assigned");
		break;
	case NET_EVENT_L4_DISCONNECTED:
		desh_print("NET_EVENT_L4_DISCONNECTED: IP down");
		break;
	case NET_EVENT_L4_IPV6_CONNECTED:
		desh_print("NET_EVENT_L4_IPV6_CONNECTED: IPv6 connectivity established");
		break;
	case NET_EVENT_L4_IPV6_DISCONNECTED:
		desh_print("NET_EVENT_L4_IPV6_DISCONNECTED: IPv6 connectivity lost");
		break;
	default:
		break;
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb, uint32_t event,
				       struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		desh_error("Fatal error received from the connectivity layer");
		return;
	}
}

#endif

/**************************************************************************************************/

#define AUTO_CONNECT_NET_MGMT_EVENTS                                                               \
	(NET_EVENT_DECT_ACTIVATE_DONE | NET_EVENT_DECT_DEACTIVATE_DONE |                           \
	 NET_EVENT_DECT_NETWORK_STATUS | NET_EVENT_DECT_SINK_STATUS)

static void auto_connect_net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
						uint32_t mgmt_event, struct net_if *iface)
{
	char err_str[128] = {0};

	switch (mgmt_event) {
	case NET_EVENT_DECT_ACTIVATE_DONE: {
		const struct dect_common_resp_evt *evt =
			(const struct dect_common_resp_evt *)cb->info;

		if (evt->status != DECT_MAC_STATUS_OK) {
			dect_shell_util_mac_error_to_string(evt->status, err_str, sizeof(err_str));
			desh_error("Activation failed: %d (%s)", evt->status, err_str);
		} else {
			desh_print("Activation done");
			if (auto_connect_sett_is_enabled()) {
				/* Trigger a work for doing the auto connect */
				desh_print("Auto connect is enabled, scheduling work in %d seconds",
					   sett_auto_connect_delay_secs);
				k_work_schedule_for_queue(&desh_common_work_q, &auto_connect_work,
							  K_SECONDS(sett_auto_connect_delay_secs));
			}
		}
		break;
	}
	case NET_EVENT_DECT_DEACTIVATE_DONE: {
		const struct dect_common_resp_evt *evt =
			(const struct dect_common_resp_evt *)cb->info;

		if (evt->status != DECT_MAC_STATUS_OK) {
			dect_shell_util_mac_error_to_string(evt->status, err_str, sizeof(err_str));
			desh_error("De-activation failed: %d (%s)", evt->status, err_str);
		} else {
			desh_print("De-activation done");
			context.connected = false;
		}
		break;
	}
	case NET_EVENT_DECT_NETWORK_STATUS: {
		const struct dect_network_status_evt *evt =
			(const struct dect_network_status_evt *)cb->info;

		if (evt->network_status == DECT_NETWORK_STATUS_CREATED) {
			desh_print("FT: network created");
			context.connected = true;
		} else if (evt->network_status == DECT_NETWORK_STATUS_REMOVED) {
			desh_print("FT: network removed");
			context.connected = false;
		} else if (evt->network_status == DECT_NETWORK_STATUS_JOINED) {
			desh_print("PT: Joined a network");
			context.connected = true;
		} else if (evt->network_status == DECT_NETWORK_STATUS_UNJOINED) {
			desh_print("PT: Joined a network");
			context.connected = true;
		} else if (evt->network_status == DECT_NETWORK_STATUS_FAILURE) {
			if (evt->dect_err_cause == DECT_MAC_STATUS_OS_ERROR) {
				desh_error("Network creation failed (OS error): %d",
					   evt->os_err_cause);
			} else {
				dect_shell_util_mac_error_to_string(evt->dect_err_cause, err_str,
								    sizeof(err_str));

				desh_error("Network creation failed: %d (%s)", evt->dect_err_cause,
					   err_str);
			}
			context.connected = false;
		} else {
			desh_error("Unknown network status: %d", evt->network_status);
		}
		break;
	}
	case NET_EVENT_DECT_SINK_STATUS: {
		const struct dect_sink_status_evt *evt =
			(const struct dect_sink_status_evt *)cb->info;

		if (evt->sink_status == DECT_SINK_STATUS_CONNECTED) {
			k_sem_give(&auto_conn_sink_connected_sem);
		} else {
			k_sem_reset(&auto_conn_sink_connected_sem);
		}
		break;
	}}
}

/******************************************************************************/

int auto_connect_sett_enable_disable(bool enabled)
{
	const char *key_enabled = AUTO_CONNECT_SETT_KEY "/" AUTO_CONNECT_SETT_ENABLED;
	int err;

	sett_auto_connect_enabled = enabled;

	desh_print("at_cmd_mode autostarting %s", ((enabled == true) ? "enabled" : "disabled"));

	err = settings_save_one(key_enabled, &sett_auto_connect_enabled,
				sizeof(sett_auto_connect_enabled));
	if (err) {
		desh_error("sett_auto_connect_enabled: err %d from settings_save_one()", err);
		return err;
	}

	return 0;
}

bool auto_connect_sett_is_enabled(void)
{
	return sett_auto_connect_enabled;
}

int auto_connect_sett_delay_get(void)
{
	return sett_auto_connect_delay_secs;
}

int auto_connect_sett_delay_set(int delay_secs)
{
	const char *key_delay = AUTO_CONNECT_SETT_KEY "/" AUTO_CONNECT_SETT_DELAY;
	int err;

	if (delay_secs < 0) {
		desh_error("Invalid delay_secs %d", delay_secs);
		return -EINVAL;
	}
	sett_auto_connect_delay_secs = delay_secs;

	err = settings_save_one(key_delay, &sett_auto_connect_delay_secs,
				sizeof(sett_auto_connect_delay_secs));
	if (err) {
		desh_error("sett_auto_connect_delay_secs: err %d from settings_save_one()", err);
		return err;
	}

	return 0;
}

static struct settings_handler cfg = {.name = AUTO_CONNECT_SETT_KEY,
				      .h_set = auto_connect_sett_handler};

/******************************************************************************/

int auto_connect_init(void)
{
	int ret;

	net_mgmt_init_event_callback(&auto_connect_net_mgmt_cb, auto_connect_net_mgmt_event_handler,
				     AUTO_CONNECT_NET_MGMT_EVENTS);
	net_mgmt_add_event_callback(&auto_connect_net_mgmt_cb);

#if defined(CONFIG_NET_CONNECTION_MANAGER)
	/* Setup handler for Zephyr NET Connection Manager events. */
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);
#endif
	context.connected = false;
	context.iface = net_if_get_by_index(
		net_if_get_by_name("nrf91_dect")); /* TODO: kconfig for the used name */
	if (!context.iface) {
		printk("Interface nrf91_dect not found\n");
	}

	/* Settings init */
	sett_auto_connect_enabled = false;
#if defined(CONFIG_DECT_NRP_MAC_BORDER_ROUTER)
	sett_auto_connect_delay_secs = 30;
#else
	sett_auto_connect_delay_secs = 0;
#endif
	ret = settings_subsys_init();
	if (ret) {
		printk("Failed to initialize settings subsystem, error: %d\n", ret);
		return ret;
	}

	ret = settings_register(&cfg);
	if (ret) {
		printk("Cannot register settings handler %d\n", ret);
		return ret;
	}

	ret = settings_load();
	if (ret) {
		printk("Cannot load settings %d\n", ret);
		return ret;
	}
	return ret;
}

SYS_INIT(auto_connect_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
