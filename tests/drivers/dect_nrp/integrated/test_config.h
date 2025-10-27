/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/net/net_if.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Forward declarations */
struct net_if;

/* Test configuration matching samples/dect/dect_mac/dect_shell/prj.conf */

/* DECT NR+ Configuration */
#define DECT_NRP_MTU		   1500
#define DECT_MAX_NETWORK_ID	   0xFFFFFFFF
#define DECT_MIN_PN		   1728
#define DECT_MAX_PN		   1919
#define DECT_DEFAULT_TX_POWER	   10
#define DECT_DEFAULT_BEACON_PERIOD 1000

/* Test runtime variables */
extern bool test_modem_initialized;
extern bool test_dect_activated;
extern bool test_network_interface_up;
extern uint32_t test_current_network_id;
extern uint16_t test_current_pn;
extern uint8_t test_current_role;

/* Test helper functions */
void test_reset_runtime_state(void);
void test_set_modem_response(int response_code);
int test_get_last_modem_call(void);

/* Mock function implementations for runtime behavior */
static inline void dect_mgmt_activate_done_evt(struct net_if *iface, int status)
{
	printk("Test: DECT activation completed with status: %d\n", status);
	test_dect_activated = (status == 0);
}

static inline void dect_mgmt_deactivate_done_evt(struct net_if *iface, int status)
{
	printk("Test: DECT deactivation completed with status: %d\n", status);
	test_dect_activated = (status != 0);
}

static inline void dect_mgmt_scan_done_evt(struct net_if *iface, void *scan_result)
{
	printk("Test: DECT scan completed\n");
}

static inline void dect_mgmt_rssi_scan_done_evt(struct net_if *iface, int status)
{
	printk("Test: RSSI scan completed with status: %d\n", status);
}

static inline void dect_mgmt_nw_beacon_start_evt(struct net_if *iface, int status)
{
	printk("Test: Beacon start completed with status: %d\n", status);
}

static inline void dect_mgmt_parent_association_created_evt(struct net_if *iface,
							    uint32_t long_rd_id)
{
	printk("Test: Parent association created with ID: 0x%08X\n", long_rd_id);
}

static inline void dect_mgmt_data_received_evt(struct net_if *iface, void *data_info)
{
	printk("Test: Data received event\n");
}

static inline void dect_mgmt_data_tx_done_evt(struct net_if *iface, int status)
{
	printk("Test: Data TX completed with status: %d\n", status);
}

/* DECT Status codes for testing */
#define DECT_STATUS_SUCCESS 0
#define DECT_STATUS_FAILURE -1
#define DECT_STATUS_TIMEOUT -2
#define DECT_STATUS_BUSY    -3

/* DECT Role definitions */
#define DECT_ROLE_FT 1
#define DECT_ROLE_PT 2

/* DECT Association types */
#define DECT_ASSOCIATION_TYPE_HANDSET 1
#define DECT_ASSOCIATION_TYPE_HEADSET 2

/* Network management event definitions */
#define NET_EVENT_DECT_ACTIVATE_DONE		  0x1001
#define NET_EVENT_DECT_DEACTIVATE_DONE		  0x1002
#define NET_EVENT_DECT_SCAN_DONE		  0x1003
#define NET_EVENT_DECT_RSSI_SCAN_DONE		  0x1004
#define NET_EVENT_DECT_BEACON_START_DONE	  0x1005
#define NET_EVENT_DECT_PARENT_ASSOCIATION_CREATED 0x1006
#define NET_EVENT_DECT_DATA_RECEIVED		  0x1007
#define NET_EVENT_DECT_DATA_TX_DONE		  0x1008

/* Network management request definitions */
#define NET_REQUEST_DECT_CMD_ACTIVATE	       0x2001
#define NET_REQUEST_DECT_CMD_DEACTIVATE	       0x2002
#define NET_REQUEST_DECT_CMD_SCAN	       0x2003
#define NET_REQUEST_DECT_CMD_RSSI_SCAN	       0x2004
#define NET_REQUEST_DECT_CMD_BEACON_START      0x2005
#define NET_REQUEST_DECT_CMD_CLUSTER_CONFIG    0x2006
#define NET_REQUEST_DECT_CMD_ASSOCIATION_SETUP 0x2007
#define NET_REQUEST_DECT_CMD_DATA_TX	       0x2008

/* Test data structures */
struct dect_mgmt_activate_params {
	uint8_t role;
	uint16_t pn;
	uint32_t network_id;
	int8_t tx_power;
};

struct dect_mgmt_deactivate_params {
	/* No parameters needed for deactivation */
	uint8_t reserved;
};

struct dect_mgmt_rssi_scan_params {
	uint32_t start_frequency;
	uint32_t end_frequency;
	uint32_t step;
	uint32_t duration;
};

struct dect_mgmt_scan_params {
	uint32_t start_frequency;
	uint32_t end_frequency;
	uint32_t step;
};

struct dect_mgmt_beacon_start_params {
	uint8_t role;
	uint16_t pn;
	uint32_t network_id;
};

struct dect_mgmt_beacon_stop_params {
	/* No parameters needed for beacon stop */
	uint8_t reserved;
};

struct dect_mgmt_data_params {
	uint32_t target_long_rd_id;
	uint8_t flow_id;
	uint8_t *data;
	size_t data_len;
};

struct dect_mgmt_association_params {
	uint32_t target_long_rd_id;
	uint8_t association_type;
};

struct dect_mgmt_cluster_params {
	uint16_t pn;
	uint32_t network_id;
	uint32_t cluster_beacon_period;
};

struct dect_scan_result {
	uint16_t channel;
	uint32_t long_rd_id;
	int8_t rssi;
};

struct dect_data_rcv_info {
	uint32_t source_long_rd_id;
	uint8_t flow_id;
	size_t data_length;
};

/* Note: Using real net_mgmt() and net_mgmt_event_notify() APIs - no mock wrappers needed */

#endif /* TEST_CONFIG_H */
