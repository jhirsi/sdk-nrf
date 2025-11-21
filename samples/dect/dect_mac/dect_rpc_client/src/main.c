/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/net/net_ip.h>
#include <nrf_rpc.h>

#include <net/l2_dect_nrp/rpc/client/dect_rpc_client.h>
#include <zephyr/net/dect_mgmt.h>

LOG_MODULE_REGISTER(dect_rpc_client_sample, LOG_LEVEL_DBG);

NRF_RPC_GROUP_DECLARE(dect_rpc_group);

static void err_handler(const struct nrf_rpc_err_report *report)
{
	LOG_ERR("nRF RPC error %d occurred. See nRF RPC logs for more details", report->code);
}

static void bound_handler(const struct nrf_rpc_group *group)
{
	static bool dect_group_initialized;

	if (group == &dect_rpc_group) {
		if (dect_group_initialized) {
			LOG_WRN("DECT RPC peer reset detected");
		} else {
			dect_group_initialized = true;
			LOG_INF("DECT RPC group bound");
		}
	}
}

static void dect_event_handler(int iface_index, uint32_t event,
				const void *event_data, size_t event_data_len)
{
	LOG_INF("DECT Event: iface_index=%d, event=0x%08x", iface_index, event);
}

int main(void)
{
	int ret;

	LOG_INF("DECT RPC Client Sample");
	LOG_INF("Initializing RPC client");

	nrf_rpc_set_bound_handler(bound_handler);

	ret = nrf_rpc_init(err_handler);
	if (ret != 0) {
		LOG_ERR("RPC init failed: %d", ret);
		return ret;
	}

	LOG_INF("Initializing DECT RPC client");

	ret = dect_rpc_client_init();
	if (ret != 0) {
		LOG_ERR("DECT RPC client init failed: %d", ret);
		return ret;
	}

	/* Register event callback */
	dect_rpc_register_event_callback(dect_event_handler, NULL);

	/* Subscribe to DECT events */
	/* Note: net_mgmt events are uint64_t, but RPC API uses uint32_t mask */
	uint32_t event_mask = (uint32_t)(NET_EVENT_DECT_ACTIVATE_DONE |
					 NET_EVENT_DECT_DEACTIVATE_DONE |
					 NET_EVENT_DECT_SCAN_RESULT |
					 NET_EVENT_DECT_SCAN_DONE |
					 NET_EVENT_DECT_ASSOCIATION_CHANGED |
					 NET_EVENT_DECT_NETWORK_STATUS |
					 NET_EVENT_DECT_SINK_STATUS);

	ret = dect_rpc_event_subscribe(0, event_mask);
	if (ret != 0) {
		LOG_WRN("Failed to subscribe to events: %d", ret);
	}

	LOG_INF("DECT RPC client ready");
	LOG_INF("Use shell commands to control DECT stack");

	return 0;
}

/* Shell commands for testing */
static int cmd_dect_activate(const struct shell *sh, size_t argc, char **argv)
{
	int ret = dect_rpc_activate(0);

	if (ret == 0) {
		shell_print(sh, "DECT stack activated");
	} else {
		shell_error(sh, "Failed to activate: %d", ret);
	}

	return ret;
}

static int cmd_dect_deactivate(const struct shell *sh, size_t argc, char **argv)
{
	int ret = dect_rpc_deactivate(0);

	if (ret == 0) {
		shell_print(sh, "DECT stack deactivated");
	} else {
		shell_error(sh, "Failed to deactivate: %d", ret);
	}

	return ret;
}

static int cmd_dect_status(const struct shell *sh, size_t argc, char **argv)
{
	struct dect_status_info status;
	char addr_str[NET_IPV6_ADDR_LEN];
	int ret = dect_rpc_status_info_get(0, &status);

	if (ret == 0) {
		shell_print(sh, "DECT Status:");
		shell_print(sh, "  Modem activated: %s", status.mdm_activated ? "yes" : "no");
		shell_print(sh, "  Cluster running: %s", status.cluster_running ? "yes" : "no");
		if (status.cluster_running) {
			shell_print(sh, "  Cluster channel: %d", status.cluster_channel);
		}
		shell_print(sh, "  Network beacon: %s", status.nw_beacon_running ? "yes" : "no");
		shell_print(sh, "  Parent count: %d", status.parent_count);
		for (uint8_t i = 0; i < status.parent_count && i < 1; i++) {
			shell_print(sh, "    Parent[%d]:", i);
			shell_print(sh, "      Long RD ID: 0x%08x", status.parent_associations[i].long_rd_id);
			if (net_addr_ntop(AF_INET6, &status.parent_associations[i].local_ipv6_addr,
					  addr_str, sizeof(addr_str))) {
				shell_print(sh, "      Local IPv6: %s", addr_str);
			}
			shell_print(sh, "      Global IPv6 set: %s",
				    status.parent_associations[i].global_ipv6_addr_set ? "yes" : "no");
			if (status.parent_associations[i].global_ipv6_addr_set) {
				if (net_addr_ntop(AF_INET6, &status.parent_associations[i].global_ipv6_addr,
						  addr_str, sizeof(addr_str))) {
					shell_print(sh, "      Global IPv6: %s", addr_str);
				}
			}
		}
		shell_print(sh, "  Child count: %d", status.child_count);
		for (uint8_t i = 0; i < status.child_count; i++) {
			shell_print(sh, "    Child[%d]:", i);
			shell_print(sh, "      Long RD ID: 0x%08x", status.child_associations[i].long_rd_id);
			if (net_addr_ntop(AF_INET6, &status.child_associations[i].local_ipv6_addr,
					  addr_str, sizeof(addr_str))) {
				shell_print(sh, "      Local IPv6: %s", addr_str);
			}
			shell_print(sh, "      Global IPv6 set: %s",
				    status.child_associations[i].global_ipv6_addr_set ? "yes" : "no");
			if (status.child_associations[i].global_ipv6_addr_set) {
				if (net_addr_ntop(AF_INET6, &status.child_associations[i].global_ipv6_addr,
						  addr_str, sizeof(addr_str))) {
					shell_print(sh, "      Global IPv6: %s", addr_str);
				}
			}
		}
		shell_print(sh, "  Border Router:");
		if (status.br_net_iface) {
			shell_print(sh, "    Interface: %p", status.br_net_iface);
		} else {
			shell_print(sh, "    Interface: none");
		}
		shell_print(sh, "    Global IPv6 prefix set: %s",
			    status.br_global_ipv6_addr_prefix_set ? "yes" : "no");
		if (status.br_global_ipv6_addr_prefix_set) {
			if (net_addr_ntop(AF_INET6, &status.br_global_ipv6_addr_prefix,
					  addr_str, sizeof(addr_str))) {
				shell_print(sh, "    Global IPv6 prefix: %s/%d", addr_str,
					    status.br_global_ipv6_addr_prefix_len);
			}
		}
		shell_print(sh, "  FW version: %s", status.fw_version_str);
	} else {
		shell_error(sh, "Failed to get status: %d", ret);
	}

	return ret;
}

static const char *region_to_str(enum dect_settings_region region)
{
	switch (region) {
	case DECT_SETTINGS_REGION_EU:
		return "EU";
	case DECT_SETTINGS_REGION_US:
		return "US";
	case DECT_SETTINGS_REGION_GLOBAL:
		return "GLOBAL";
	default:
		return "UNKNOWN";
	}
}

static const char *cluster_beacon_period_to_str(enum dect_cluster_beacon_period period)
{
	switch (period) {
	case DECT_MAC_CLUSTER_BEACON_PERIOD_10MS:
		return "10ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_50MS:
		return "50ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_100MS:
		return "100ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_500MS:
		return "500ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_1000MS:
		return "1000ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_1500MS:
		return "1500ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_2000MS:
		return "2000ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_4000MS:
		return "4000ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_8000MS:
		return "8000ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_16000MS:
		return "16000ms";
	case DECT_MAC_CLUSTER_BEACON_PERIOD_32000MS:
		return "32000ms";
	default:
		return "UNKNOWN";
	}
}

static const char *nw_beacon_period_to_str(enum dect_nw_beacon_period period)
{
	switch (period) {
	case DECT_MAC_NW_BEACON_PERIOD_50MS:
		return "50ms";
	case DECT_MAC_NW_BEACON_PERIOD_100MS:
		return "100ms";
	case DECT_MAC_NW_BEACON_PERIOD_500MS:
		return "500ms";
	case DECT_MAC_NW_BEACON_PERIOD_1000MS:
		return "1000ms";
	case DECT_MAC_NW_BEACON_PERIOD_1500MS:
		return "1500ms";
	case DECT_MAC_NW_BEACON_PERIOD_2000MS:
		return "2000ms";
	case DECT_MAC_NW_BEACON_PERIOD_4000MS:
		return "4000ms";
	default:
		return "UNKNOWN";
	}
}

static const char *security_mode_to_str(enum dect_mac_security_mode mode)
{
	switch (mode) {
	case DECT_MAC_SECURITY_MODE_NONE:
		return "NONE";
	case DECT_MAC_SECURITY_MODE_1:
		return "MODE_1";
	default:
		return "UNKNOWN";
	}
}

static void print_hex_key(const struct shell *sh, const char *label, const uint8_t *key, size_t len)
{
	char hex_str[64]; /* Enough for 16 bytes * 2 + 1 */
	char *p = hex_str;

	if (len * 2 + 1 > sizeof(hex_str)) {
		shell_print(sh, "      %s: (key too long)", label);
		return;
	}

	for (size_t i = 0; i < len; i++) {
		p += sprintf(p, "%02x", key[i]);
	}
	*p = '\0';
	shell_print(sh, "      %s: %s", label, hex_str);
}

static int cmd_dect_settings_read(const struct shell *sh, size_t argc, char **argv)
{
	struct dect_settings settings;
	int ret = dect_rpc_settings_read(0, &settings);

	if (ret == 0) {
		shell_print(sh, "DECT Settings:");
		shell_print(sh, "  Command params:");
		shell_print(sh, "    Reset to driver defaults: %s",
			    settings.cmd_params.reset_to_driver_defaults ? "yes" : "no");
		shell_print(sh, "    Write scope bitmap: 0x%04x", settings.cmd_params.write_scope_bitmap);
		shell_print(sh, "  Auto-start:");
		shell_print(sh, "    Activate: %s", settings.auto_start.activate ? "yes" : "no");
		shell_print(sh, "  Region: %s (%d)", region_to_str(settings.region), settings.region);
		shell_print(sh, "  Device type: 0x%08x", settings.device_type);
		if (settings.device_type & DECT_DEVICE_TYPE_FT) {
			shell_print(sh, "    FT (Fixed Terminal)");
		}
		if (settings.device_type & DECT_DEVICE_TYPE_PT) {
			shell_print(sh, "    PT (Portable Terminal)");
		}
		shell_print(sh, "  Identities:");
		shell_print(sh, "    Network ID: 0x%08x", settings.identities.network_id);
		shell_print(sh, "    Transmitter Long RD ID: 0x%08x",
			    settings.identities.transmitter_long_rd_id);
		shell_print(sh, "  TX settings:");
		shell_print(sh, "    Max power: %d dBm", settings.tx.max_power_dbm);
		shell_print(sh, "    Max MCS: %d", settings.tx.max_mcs);
		shell_print(sh, "  Power save: %s", settings.power_save ? "enabled" : "disabled");
		shell_print(sh, "  Band: %d", settings.band_nbr);
		shell_print(sh, "  RSSI scan:");
		shell_print(sh, "    Scan suitable percent: %d%%", settings.rssi_scan.scan_suitable_percent);
		shell_print(sh, "    Time per channel: %d ms", settings.rssi_scan.time_per_channel_ms);
		shell_print(sh, "    Free threshold: %d dBm", settings.rssi_scan.free_threshold_dbm);
		shell_print(sh, "    Busy threshold: %d dBm", settings.rssi_scan.busy_threshold_dbm);
		shell_print(sh, "  Cluster settings:");
		shell_print(sh, "    Max beacon TX power: %d dBm",
			    settings.cluster.max_beacon_tx_power_dbm);
		shell_print(sh, "    Max cluster power: %d dBm", settings.cluster.max_cluster_power_dbm);
		shell_print(sh, "    Beacon period: %s (%d)", cluster_beacon_period_to_str(settings.cluster.beacon_period),
			    settings.cluster.beacon_period);
		shell_print(sh, "    Max num neighbors: %d", settings.cluster.max_num_neighbors);
		shell_print(sh, "    Neighbor inactivity timer: %d ms",
			    settings.cluster.neighbor_inactivity_disconnect_timer_ms);
		shell_print(sh, "    Channel loaded percent: %d%%", settings.cluster.channel_loaded_percent);
		shell_print(sh, "  Network beacon:");
		if (settings.nw_beacon.channel == DECT_MAC_NW_BEACON_CHANNEL_NOT_USED) {
			shell_print(sh, "    Channel: NOT_USED");
		} else {
			shell_print(sh, "    Channel: %d", settings.nw_beacon.channel);
		}
		shell_print(sh, "    Beacon period: %s (%d)", nw_beacon_period_to_str(settings.nw_beacon.beacon_period),
			    settings.nw_beacon.beacon_period);
		shell_print(sh, "  Association:");
		shell_print(sh, "    Max beacon RX failures: %d", settings.association.max_beacon_rx_failures);
		shell_print(sh, "    Min sensitivity: %d dBm", settings.association.min_sensitivity_dbm);
		shell_print(sh, "  Network join:");
		if (settings.network_join.target_ft_long_rd_id == DECT_SETT_NETWORK_JOIN_TARGET_FT_ANY) {
			shell_print(sh, "    Target FT Long RD ID: ANY (0x%08x)",
				    settings.network_join.target_ft_long_rd_id);
		} else {
			shell_print(sh, "    Target FT Long RD ID: 0x%08x",
				    settings.network_join.target_ft_long_rd_id);
		}
		shell_print(sh, "  Security:");
		shell_print(sh, "    Mode: %s (%d)", security_mode_to_str(settings.sec_conf.mode),
			    settings.sec_conf.mode);
		print_hex_key(sh, "Integrity key", settings.sec_conf.integrity_key,
			      DECT_MAC_INTEGRITY_KEY_LENGTH);
		print_hex_key(sh, "Cipher key", settings.sec_conf.cipher_key,
			      DECT_MAC_CIPHER_KEY_LENGTH);
	} else {
		shell_error(sh, "Failed to read settings: %d", ret);
	}

	return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(dect_cmds,
	SHELL_CMD(activate, NULL, "Activate DECT stack", cmd_dect_activate),
	SHELL_CMD(deactivate, NULL, "Deactivate DECT stack", cmd_dect_deactivate),
	SHELL_CMD(status, NULL, "Get DECT status", cmd_dect_status),
	SHELL_CMD(settings_read, NULL, "Read DECT settings", cmd_dect_settings_read),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(dect, &dect_cmds, "DECT RPC commands", NULL);

