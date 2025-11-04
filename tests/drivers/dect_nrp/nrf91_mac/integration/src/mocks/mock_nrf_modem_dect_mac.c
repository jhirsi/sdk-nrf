/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
 * Mock implementation for nrf_modem_dect_mac.h using real headers
 * This provides mock implementations of the actual modem API functions
 */

#include <nrf_modem_dect_mac.h>
#include <nrf_modem.h>
#include "unity.h"
#include <string.h>
#include <stdbool.h>
#include <zephyr/linker/sections.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/* Mock modem info enum - simplified version for testing */
enum modem_info {
	MODEM_INFO_FW_VERSION = 1
	/* Add other values as needed */
};

LOG_MODULE_REGISTER(mock_nrf_modem_dect_mac, LOG_LEVEL_INF);

/* Mock state tracking */
static bool mock_modem_initialized;
static bool mock_dect_activated;
struct nrf_modem_dect_mac_op_callbacks mock_op_callbacks;
struct nrf_modem_dect_mac_ntf_callbacks mock_ntf_callbacks;

/* Async callback simulation using timer-based approach */
struct async_callback_work_item {
	void (*callback_func)(void *arg);
	void *params;
	uint8_t param_data[256]; /* Buffer to store callback parameters */
};

static void async_callback_timer_handler(struct k_timer *timer)
{
	struct async_callback_work_item *work_item =
		(struct async_callback_work_item *)k_timer_user_data_get(timer);

	if (work_item && work_item->callback_func && work_item->params) {
		/* Execute the callback */
		work_item->callback_func(work_item->params);
	}

	/* Clean up dynamically allocated memory */
	if (work_item) {
		k_free(work_item);
	}
	k_free(timer);
}

/* Helper function to simulate asynchronous callback with timer-based delay */
void simulate_async_callback(void (*callback_func)(void *), void *params)
{
	/* Use dynamic memory allocation to avoid static variable conflicts */
	struct async_callback_work_item *work_item =
		k_malloc(sizeof(struct async_callback_work_item));
	struct k_timer *async_timer = k_malloc(sizeof(struct k_timer));

	if (callback_func && params && work_item && async_timer) {
		/* Store callback and params for timer handler */
		work_item->callback_func = callback_func;

		/* Copy parameters to our buffer - use actual size for capability_ntf or max size */
		size_t param_size;

		if (callback_func == (void (*)(void *))mock_ntf_callbacks.capability_ntf) {
			/* capability_ntf_cb_params is larger, use sizeof */
			param_size = sizeof(struct nrf_modem_dect_mac_capability_ntf_cb_params);
		} else {
			/* Default size for other callback parameter structures */
			param_size = 64;
		}

		if (param_size <= sizeof(work_item->param_data)) {
			memcpy(work_item->param_data, params, param_size);
			work_item->params = (void *)work_item->param_data;

			/* Initialize timer with work item as user data */
			k_timer_init(async_timer, async_callback_timer_handler, NULL);
			k_timer_user_data_set(async_timer, work_item);

			/* Schedule callback execution after short delay to simulate async behavior
			 */
			k_timer_start(async_timer, K_MSEC(5), K_NO_WAIT);
		} else {
			/* Cleanup on parameter size error */
			k_free(work_item);
			k_free(async_timer);
		}
	} else {
		/* Cleanup on allocation failure */
		if (work_item) {
			k_free(work_item);
		}
		if (async_timer) {
			k_free(async_timer);
		}
	}
}

/* Note: We don't access driver semaphores directly - the driver's own callbacks handle that */

/* CMock-style call counters */
int mock_nrf_modem_dect_mac_callback_set_call_count;
int mock_nrf_modem_dect_mac_network_scan_call_count;
int mock_nrf_modem_dect_mac_cluster_beacon_receive_call_count;
int mock_nrf_modem_dect_mac_association_call_count;
int mock_nrf_modem_dect_mac_association_release_call_count;
int mock_nrf_modem_dect_mac_neighbor_list_call_count;
/* Mock neighbor info call counter */
int mock_nrf_modem_dect_mac_neighbor_info_call_count;
int mock_nrf_modem_dect_control_systemmode_set_call_count;
int mock_nrf_modem_dect_control_configure_call_count;
int mock_nrf_modem_dect_control_functional_mode_set_call_count;
int mock_nrf_modem_dect_mac_rssi_scan_call_count;

/* Remove the extern - this is where we define the variables */

/* Expected return values for CMock-style functions */
static int expected_callback_set_return;
static int expected_network_scan_return;
static int expected_cluster_beacon_receive_return;

/* Mock function implementations */

int nrf_modem_dect_mac_callback_set(const struct nrf_modem_dect_mac_op_callbacks *op_cb,
				    const struct nrf_modem_dect_mac_ntf_callbacks *ntf_cb)
{
	mock_nrf_modem_dect_mac_callback_set_call_count++;

	printk("MOCK: callback_set called, op_cb=%p, ntf_cb=%p\n", op_cb, ntf_cb);

	if (op_cb) {
		memcpy(&mock_op_callbacks, op_cb, sizeof(mock_op_callbacks));
		printk("MOCK: Copied operation callbacks\n");
	}
	if (ntf_cb) {
		memcpy(&mock_ntf_callbacks, ntf_cb, sizeof(mock_ntf_callbacks));
		printk("MOCK: Copied notification callbacks, cluster_beacon_ntf=%p\n",
		       mock_ntf_callbacks.cluster_beacon_ntf);
	}
	mock_modem_initialized = true;
	return expected_callback_set_return;
}

int nrf_modem_dect_mac_network_scan(struct nrf_modem_dect_mac_network_scan_params *params)
{
	mock_nrf_modem_dect_mac_network_scan_call_count++;

	/* Check if the DECT stack is activated */
	if (!mock_dect_activated) {
		/* Stack is deactivated - simulate immediate failure via async callback */
		if (mock_op_callbacks.network_scan) {
			struct nrf_modem_dect_mac_network_scan_cb_params scan_fail_params = {
				.status = NRF_MODEM_DECT_MAC_STATUS_NOT_ALLOWED,
				.num_scanned_channels = 0
			};
			LOG_DBG("MOCK: Network scan not allowed - DECT stack is deactivated");
			simulate_async_callback((void (*)(void *))mock_op_callbacks.network_scan,
						&scan_fail_params);
		}
		return 0; /* Request accepted but will fail asynchronously */
	}

	/* Stack is activated - normal operation */
	/* For proper async testing, we don't auto-trigger callbacks here.
	 * The test will manually trigger:
	 * 1. ntf_callbacks.cluster_beacon_ntf (simulate beacon reception)
	 * 2. op_callbacks.network_scan (simulate scan completion)
	 */

	return expected_network_scan_return;
}

int nrf_modem_dect_mac_cluster_beacon_receive(
	struct nrf_modem_dect_mac_cluster_beacon_receive_params *params)
{
	mock_nrf_modem_dect_mac_cluster_beacon_receive_call_count++;
	mock_dect_activated = true;
	return expected_cluster_beacon_receive_return;
}

int nrf_modem_dect_mac_association(struct nrf_modem_dect_mac_association_params *params)
{
	mock_nrf_modem_dect_mac_association_call_count++;
	LOG_DBG("MOCK: nrf_modem_dect_mac_association called with long_rd_id=0x%08X, "
		"network_id=0x%08X",
		params->long_rd_id, params->network_id);

	/* Check if the DECT stack is activated */
	if (!mock_dect_activated) {
		/* Stack is deactivated - simulate association failure via async callback */
		if (mock_op_callbacks.association) {
			struct nrf_modem_dect_mac_association_cb_params assoc_fail_params = {
				.status = NRF_MODEM_DECT_MAC_STATUS_NOT_ALLOWED,
				.long_rd_id = params->long_rd_id};
			LOG_DBG("MOCK: Association not allowed - DECT stack is deactivated");
			simulate_async_callback((void (*)(void *))mock_op_callbacks.association,
						&assoc_fail_params);
		}
		return 0; /* Request accepted but will fail asynchronously */
	}

	return 0;
}

int nrf_modem_dect_mac_rssi_scan(struct nrf_modem_dect_mac_rssi_scan_params *params)
{
	mock_nrf_modem_dect_mac_rssi_scan_call_count++;

	LOG_DBG("MOCK: nrf_modem_dect_mac_rssi_scan called with channel_scan_length=%d, "
		"num_channels=%d, band=%d",
		params ? params->channel_scan_length : 0,
		params ? params->num_channels : 0,
		params ? params->band : 0);

	/* Check if the DECT stack is activated */
	if (!mock_dect_activated) {
		/* Stack is deactivated - simulate immediate failure via async callback */
		if (mock_op_callbacks.rssi_scan) {
			struct nrf_modem_dect_mac_rssi_scan_cb_params rssi_fail_params = {
				.status = NRF_MODEM_DECT_MAC_STATUS_NOT_ALLOWED
			};

			LOG_DBG("MOCK: RSSI scan not allowed - DECT stack is deactivated");
			simulate_async_callback((void (*)(void *))mock_op_callbacks.rssi_scan,
						&rssi_fail_params);
		}
		return 0; /* Request accepted but will fail asynchronously */
	}

	/* Stack is activated - simulate successful RSSI scan with async callbacks */
	if (params && params->num_channels > 0 && params->channel_list) {
		/* Simulate RSSI scan notification callback for each channel */
		/* For test purposes, simulate one result */
		if (mock_ntf_callbacks.rssi_scan_ntf) {
			/* Allocate arrays for busy, possible, free subslots */
			static uint8_t busy_array[6] = {0};
			static uint8_t possible_array[6] = {0};
			static uint8_t free_array[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

			struct nrf_modem_dect_mac_rssi_scan_ntf_cb_params ntf_params = {
				.channel = params->channel_list[0],
				.busy_percentage = 10, /* 10% busy */
				.rssi_meas_array_size = 6,
				.busy = busy_array,
				.possible = possible_array,
				.free = free_array
			};

			LOG_DBG("MOCK: Simulating rssi_scan_ntf callback for channel %d",
				ntf_params.channel);
			simulate_async_callback(
				(void (*)(void *))mock_ntf_callbacks.rssi_scan_ntf, &ntf_params);
		}

		/* Simulate RSSI scan completion callback */
		if (mock_op_callbacks.rssi_scan) {
			struct nrf_modem_dect_mac_rssi_scan_cb_params rssi_done_params = {
				.status = NRF_MODEM_DECT_MAC_STATUS_OK
			};

			LOG_DBG("MOCK: Simulating rssi_scan op callback with success status");
			simulate_async_callback((void (*)(void *))mock_op_callbacks.rssi_scan,
						&rssi_done_params);
		}
	}

	return 0;
}

int nrf_modem_dect_mac_association_release(
	struct nrf_modem_dect_mac_association_release_params *params)
{
	mock_nrf_modem_dect_mac_association_release_call_count++;
	LOG_DBG("MOCK: nrf_modem_dect_mac_association_release called with long_rd_id=0x%08X, "
		"release_cause=%d",
		params->long_rd_id, params->release_cause);
	return 0;
}

/* Stub implementations for other MAC functions */

int nrf_modem_dect_mac_cluster_beacon_receive_stop(void)
{
	return 0;
}

int nrf_modem_dect_mac_cluster_configure(struct nrf_modem_dect_mac_cluster_configure_params *params)
{
	return 0;
}

int nrf_modem_dect_mac_network_scan_stop(void)
{
	return 0;
}

int nrf_modem_dect_mac_rssi_scan_stop(void)
{
	return 0;
}

int nrf_modem_dect_mac_network_beacon_configure(
	struct nrf_modem_dect_mac_network_beacon_configure_params *params)
{
	return 0;
}

int nrf_modem_dect_mac_cluster_info(void)
{
	return 0;
}

int nrf_modem_dect_mac_neighbor_info(struct nrf_modem_dect_mac_neighbor_info_params *params)
{
	mock_nrf_modem_dect_mac_neighbor_info_call_count++;

	if (params) {
		LOG_DBG("MOCK: nrf_modem_dect_mac_neighbor_info called with long_rd_id=0x%08X",
			params->long_rd_id);
	} else {
		LOG_DBG("MOCK: nrf_modem_dect_mac_neighbor_info called with NULL params");
	}

	if (mock_modem_initialized) {
		/* Simulate successful neighbor info request - will call back later */
		return 0;
	}

	return -1;
}

/* Mock neighbor list function */
int nrf_modem_dect_mac_neighbor_list(void)
{
	mock_nrf_modem_dect_mac_neighbor_list_call_count++;

	LOG_DBG("MOCK: nrf_modem_dect_mac_neighbor_list called");

	if (mock_modem_initialized) {
		/* Simulate successful neighbor list request - will call back later */
		return 0;
	}

	return -1;
}

/* Simulate NRF_MODEM_LIB_ON_INIT callback system - now using direct function call */
int nrf_modem_dect_dlc_data_tx(struct nrf_modem_dect_dlc_data_tx_params *params)
{
	return 0;
}

int nrf_modem_dect_dlc_data_discard(struct nrf_modem_dect_dlc_data_discard_params *params)
{
	return 0;
}

/* NRF Modem DECT Control functions needed by MAC driver */
int nrf_modem_dect_control_configure(struct nrf_modem_dect_control_configure_params *params)
{
	mock_nrf_modem_dect_control_configure_call_count++;

	/* Simulate asynchronous callback after successful configuration */
	if (mock_op_callbacks.control_configure) {
		struct nrf_modem_dect_mac_control_configure_cb_params cb_params = {
			.status = 0 /* Success */
		};
		simulate_async_callback((void (*)(void *))mock_op_callbacks.control_configure,
					&cb_params);
	}
	return 0;
}

int nrf_modem_dect_control_functional_mode_set(enum nrf_modem_dect_control_functional_mode mode)
{
	mock_nrf_modem_dect_control_functional_mode_set_call_count++;

	LOG_DBG("MOCK: nrf_modem_dect_control_functional_mode_set called with mode=%d", mode);

	/* Simulate successful functional mode change */
	if (mode == NRF_MODEM_DECT_CONTROL_FUNCTIONAL_MODE_ACTIVATE) {
		mock_dect_activated = true; /* Update activation state */
		/* Simulate asynchronous callback after successful activation */
		if (mock_op_callbacks.control_functional_mode) {
			struct nrf_modem_dect_mac_control_functional_mode_cb_params cb_params = {
				.status = NRF_MODEM_DECT_MAC_STATUS_OK
			};
			simulate_async_callback(
				(void (*)(void *))mock_op_callbacks.control_functional_mode,
				&cb_params);
		}
		return 0;
	} else if (mode == NRF_MODEM_DECT_CONTROL_FUNCTIONAL_MODE_DEACTIVATE) {
		mock_dect_activated = false; /* Update activation state */
		/* Simulate asynchronous callback after successful deactivation */
		if (mock_op_callbacks.control_functional_mode) {
			struct nrf_modem_dect_mac_control_functional_mode_cb_params cb_params = {
				.status = NRF_MODEM_DECT_MAC_STATUS_OK
			};

			LOG_DBG("Calling control_functional_mode callback for DEACTIVATE");
			simulate_async_callback(
				(void (*)(void *))mock_op_callbacks.control_functional_mode,
				&cb_params);
		}
		return 0;
	}
	return 0;
}

/* CMock-style expectation functions */
void mock_nrf_modem_dect_mac_callback_set_ExpectAndReturn(int return_value)
{
	expected_callback_set_return = return_value;
}

void mock_nrf_modem_dect_mac_network_scan_ExpectAndReturn(int return_value)
{
	expected_network_scan_return = return_value;
}

void mock_nrf_modem_dect_mac_cluster_beacon_receive_ExpectAndReturn(int return_value)
{
	expected_cluster_beacon_receive_return = return_value;
}

void mock_nrf_modem_dect_mac_reset(void)
{
	/* Reset call counters and state, but preserve registered callbacks */
	mock_nrf_modem_dect_mac_callback_set_call_count = 0;
	mock_nrf_modem_dect_mac_network_scan_call_count = 0;
	mock_nrf_modem_dect_mac_cluster_beacon_receive_call_count = 0;
	mock_nrf_modem_dect_mac_association_call_count = 0;
	mock_nrf_modem_dect_mac_neighbor_list_call_count = 0;
	mock_nrf_modem_dect_mac_neighbor_info_call_count = 0;
	expected_callback_set_return = 0;
	expected_network_scan_return = 0;
	expected_cluster_beacon_receive_return = 0;

	/* Note: We do NOT reset mock_op_callbacks and mock_ntf_callbacks here
	 * because they represent the real DECT driver's callback registration
	 * which should persist across test cases. The driver registers these
	 * once during initialization and they remain valid.
	 */
}

/* Simulate cluster beacon reception for testing */
void mock_simulate_cluster_beacon_received(void)
{
	if (mock_ntf_callbacks.cluster_beacon_ntf) {
		struct nrf_modem_dect_mac_cluster_beacon_ntf_cb_params beacon_params = {
			.channel = 1722, /* Band 1 channel */
			.transmitter_short_rd_id = 0x1234,
			.transmitter_long_rd_id = 0x12345678,
			.network_id = 1,
			.number_of_ies = 0,
			.ies = NULL};
		mock_ntf_callbacks.cluster_beacon_ntf(&beacon_params);
	}
}

/*
 * Note: The MAC driver registers its initialization callback using:
 * NRF_MODEM_LIB_ON_INIT(dect_nrf91_ctrl_api_init_hook, dect_nrf91_ctrl_on_modem_lib_init, NULL)
 *
 * This callback is automatically executed during system initialization, and it calls
 * nrf_modem_dect_mac_callback_set() which we mock below.
 *
 * For testing purposes, we don't need to explicitly simulate nrf_modem_lib_init() -
 * the MAC driver's NRF_MODEM_LIB_ON_INIT callback will execute during system startup.
 */

/* Mock control functions needed for initialization */

/* Mock control functions for additional initialization functions */

int nrf_modem_dect_control_systemmode_set(enum nrf_modem_dect_control_systemmode mode)
{
	mock_nrf_modem_dect_control_systemmode_set_call_count++;

	/* Simulate success for MAC mode */
	if (mode == NRF_MODEM_DECT_MODE_MAC) {
		/* First, simulate capability_ntf callback before op callback */
		if (mock_ntf_callbacks.capability_ntf) {
			struct nrf_modem_dect_mac_capability_ntf_cb_params capability_params = {
				.max_mcs = 4,
				.num_band_info_elems = 1,
				.band_info_elems = {
					[0] = {
						.band_group_index =
						NRF_MODEM_DECT_MAC_PHY_BAND_GROUP_IDX0,
						.band = NRF_MODEM_DECT_MAC_PHY_BAND1,
						.power_class = 3,
						.min_carrier = 1657,
						.max_carrier = 1677
					}
				}
			};
			simulate_async_callback(
				(void (*)(void *))mock_ntf_callbacks.capability_ntf,
				&capability_params);
		}

		/* Then, simulate asynchronous op callback after successful operation */
		if (mock_op_callbacks.control_systemmode) {
			struct nrf_modem_dect_mac_control_systemmode_cb_params cb_params = {
				.status = 0 /* Success */
			};
			simulate_async_callback(
				(void (*)(void *))mock_op_callbacks
					.control_systemmode, &cb_params);
		}
		return 0;
	}
	return -1;
}

int mock_nrf_modem_dect_control_systemmode_set_ExpectAndReturn(int return_value)
{
	/* Mock expectation function - for now just return the expected value */
	return return_value;
}

/* Note: nrf_modem_dect_mac_configure does not exist in the API - removed */

/* Simulate NRF_MODEM_LIB_ON_INIT callback system - now using direct function call */
void mock_simulate_nrf_modem_lib_init(void)
{
	/* Set mock modem as initialized */
	mock_modem_initialized = true;

	/* Note: Tests can now directly call dect_nrf91_ctrl_on_modem_lib_init()
	 * since it's been made non-static for testing.
	 */
}
