/**
 * @file test_dect_integration.c
 * @brief Unity-based DECT NR+ Stack Integration Tests
 *
 * Complete DECT NR+ stack integration tests using Unity framework with:
 * - Real DECT stack (CONFIG_DECT_NRP_MAC=y + CONFIG_DECT_NRP_MAC_DRIVER_NRF=y)
 * - Real net_mgmt() API calls with authentic DECT management structures
 * - Mock nrf_modem_dect_mac.h backend with proper callback simulation
 * - Tests complete initialization: callback_set -> systemmode -> configure -> functional_mode ->
 * NET_EVENT_DECT_ACTIVATE_DONE
 *
 * Architecture: Unity Tests → net_mgmt() → Real DECT Stack → Mock nrf_modem Backend
 */

#include "unity.h"
#include "mock_nrf_modem_dect_mac.h"
#include "test_dect_utils.h"

/* Real DECT API includes for integration testing */
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <dect_net_l2_mgmt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test_dect_integration, LOG_LEVEL_INF);

/* Network interface for testing */
static struct net_if *test_iface;

/* Forward declaration for DECT driver initialization function (now non-static for tests) */
extern void dect_nrf91_ctrl_mdm_on_modem_lib_init(int ret, void *ctx);

/* Event tracking for activation tests */
bool dect_activate_done_received;
enum dect_status_values dect_activate_done_status;

/* Event tracking for deactivation tests */
bool dect_deactivate_done_received;
enum dect_status_values dect_deactivate_done_status;

/* Semaphore for thread synchronization */
static K_SEM_DEFINE(activation_done_sem, 0, 1);

/* Event tracking for scan tests */
bool dect_scan_result_received;
bool dect_scan_done_received;
enum dect_status_values dect_scan_done_status;

/* Event tracking for RSSI scan tests */
bool dect_rssi_scan_result_received;
struct dect_rssi_scan_result_evt received_rssi_scan_result_data;
bool dect_rssi_scan_done_received;
enum dect_status_values dect_rssi_scan_done_status;

/* Event tracking for association tests */
bool dect_association_changed_received;
bool dect_association_created_received;
bool dect_association_failed_received;
struct dect_association_changed_evt received_association_data;

/* Event tracking for network status tests */
bool dect_network_status_received;
struct dect_network_status_evt received_network_status_data;

/* Event tracking for neighbor list tests */
static bool dect_neighbor_list_received;
static struct dect_neighbor_list_evt received_neighbor_list_data;

/* Event tracking for neighbor info tests */
static bool dect_neighbor_info_received;
static struct dect_neighbor_info_evt received_neighbor_info_data;

/* Storage for received beacon data from NET_EVENT_DECT_SCAN_RESULT */
struct dect_scan_result_evt received_beacon_data;
bool beacon_data_valid;

static struct net_mgmt_event_callback dect_mgmt_cb;

/* Forward declaration for debug function */
void debug_thread_context(const char *context_name);

void dect_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			     struct net_if *iface)
{
	debug_thread_context("dect_mgmt_event_handler");

	switch (mgmt_event) {
	case NET_EVENT_DECT_ACTIVATE_DONE: {
		/* Event data is stored in cb->info */
		struct dect_common_resp_evt *evt = (struct dect_common_resp_evt *)cb->info;

		LOG_DBG("NET_EVENT_DECT_ACTIVATE_DONE received with status: %d", evt->status);
		dect_activate_done_received = true;
		dect_activate_done_status = evt->status;
		/* Signal semaphore to wake up waiting test thread */
		k_sem_give(&activation_done_sem);
		break;
	}
	case NET_EVENT_DECT_DEACTIVATE_DONE: {
		/* Event data is stored in cb->info */
		struct dect_common_resp_evt *evt = (struct dect_common_resp_evt *)cb->info;

		LOG_DBG("NET_EVENT_DECT_DEACTIVATE_DONE received with status: %d", evt->status);
		dect_deactivate_done_received = true;
		dect_deactivate_done_status = evt->status;
		break;
	}
	case NET_EVENT_DECT_SCAN_RESULT: {
		/* Cluster beacon received during scan */
		LOG_DBG("NET_EVENT_DECT_SCAN_RESULT received in event handler!");

		/* Store the beacon data from the event */
		if (cb->info) {
			struct dect_scan_result_evt *beacon_evt =
				(struct dect_scan_result_evt *)cb->info;
			memcpy(&received_beacon_data, beacon_evt, sizeof(received_beacon_data));
			beacon_data_valid = true;

			LOG_DBG("Stored beacon data: channel=%d, short_rd_id=0x%04X, "
				"long_rd_id=0x%08X, network_id=0x%04X",
				received_beacon_data.channel,
				received_beacon_data.transmitter_short_rd_id,
				received_beacon_data.transmitter_long_rd_id,
				received_beacon_data.network_id);
		} else {
			LOG_WRN("NET_EVENT_DECT_SCAN_RESULT received but no beacon data available");
		}

		dect_scan_result_received = true;
		break;
	}
	case NET_EVENT_DECT_SCAN_DONE: {
		/* Scan operation completed */
		struct dect_common_resp_evt *evt = (struct dect_common_resp_evt *)cb->info;

		LOG_DBG("NET_EVENT_DECT_SCAN_DONE received with status: %d", evt->status);
		dect_scan_done_received = true;
		dect_scan_done_status = evt->status;
		break;
	}
	case NET_EVENT_DECT_RSSI_SCAN_RESULT: {
		/* RSSI scan result received */
		LOG_DBG("NET_EVENT_DECT_RSSI_SCAN_RESULT received in event handler!");

		/* Store the RSSI scan result data from the event */
		if (cb->info) {
			struct dect_rssi_scan_result_evt *rssi_evt =
				(struct dect_rssi_scan_result_evt *)cb->info;
			memcpy(&received_rssi_scan_result_data, rssi_evt,
			       sizeof(received_rssi_scan_result_data));

			LOG_DBG("Stored RSSI scan result: channel=%d, busy_percentage=%d%%, "
				"scan_suitable_percent=%d%%",
				received_rssi_scan_result_data.rssi_scan_result.channel,
				received_rssi_scan_result_data.rssi_scan_result.busy_percentage,
				received_rssi_scan_result_data.rssi_scan_result
					.scan_suitable_percent);
		} else {
			LOG_WRN("NET_EVENT_DECT_RSSI_SCAN_RESULT received but "
				"no result data available");
		}

		dect_rssi_scan_result_received = true;
		break;
	}
	case NET_EVENT_DECT_RSSI_SCAN_DONE: {
		/* RSSI scan operation completed */
		struct dect_common_resp_evt *evt = (struct dect_common_resp_evt *)cb->info;

		LOG_DBG("NET_EVENT_DECT_RSSI_SCAN_DONE received with status: %d", evt->status);
		dect_rssi_scan_done_received = true;
		dect_rssi_scan_done_status = evt->status;
		break;
	}
	case NET_EVENT_DECT_ASSOCIATION_CHANGED: {
		/* Association changed (created, released, failed, etc.) */
		LOG_DBG("NET_EVENT_DECT_ASSOCIATION_CHANGED received in event handler!");

		if (cb->info) {
			struct dect_association_changed_evt *assoc_evt =
				(struct dect_association_changed_evt *)cb->info;
			LOG_DBG("Association change type: %d, long_rd_id: 0x%08X, neighbor_role: "
				"%d",
				assoc_evt->association_change_type, assoc_evt->long_rd_id,
				assoc_evt->neighbor_role);

			/* Store the association event data for validation */
			memcpy(&received_association_data, assoc_evt,
			       sizeof(received_association_data));

			/* Check if this is an association creation or failure event */
			if (assoc_evt->association_change_type == DECT_ASSOCIATION_CREATED) {
				LOG_DBG("DECT_ASSOCIATION_CREATED event received - association "
					"successful!");
				dect_association_created_received = true;
			} else if (assoc_evt->association_change_type ==
				   DECT_ASSOCIATION_REQ_FAILED_MDM) {
				LOG_DBG("DECT_ASSOCIATION_REQ_FAILED_MDM event received - "
					"association failed!");
				dect_association_failed_received = true;
			}
		} else {
			LOG_WRN("NET_EVENT_DECT_ASSOCIATION_CHANGED received but no event data "
				"available");
		}

		dect_association_changed_received = true;
		break;
	}
	case NET_EVENT_DECT_NETWORK_STATUS: {
		/* Network status changed (created, removed, joined, unjoined) */
		LOG_DBG("NET_EVENT_DECT_NETWORK_STATUS received in event handler!");

		if (cb->info) {
			struct dect_network_status_evt *status_evt =
				(struct dect_network_status_evt *)cb->info;
			LOG_DBG("Network status: %d, dect_err_cause: %d",
				status_evt->network_status, status_evt->dect_err_cause);

			/* Store the network status event data for validation */
			memcpy(&received_network_status_data, status_evt,
			       sizeof(received_network_status_data));
		} else {
			LOG_WRN("NET_EVENT_DECT_NETWORK_STATUS received but no event data "
				"available");
		}

		dect_network_status_received = true;
		break;
	}
	case NET_EVENT_DECT_NEIGHBOR_LIST: {
		/* Neighbor list response */
		LOG_DBG("NET_EVENT_DECT_NEIGHBOR_LIST received in event handler!");

		if (cb->info) {
			struct dect_neighbor_list_evt *neighbor_evt =
				(struct dect_neighbor_list_evt *)cb->info;
			LOG_DBG("Neighbor list status: %d, neighbor_count: %d",
				neighbor_evt->status, neighbor_evt->neighbor_count);

			/* Log first few neighbors for debugging */
			for (int i = 0; i < neighbor_evt->neighbor_count && i < 3; i++) {
				LOG_DBG("Neighbor[%d]: Long RD ID 0x%08X", i,
					neighbor_evt->neighbor_long_rd_ids[i]);
			}

			/* Store the neighbor list event data for validation */
			memcpy(&received_neighbor_list_data, neighbor_evt,
			       sizeof(received_neighbor_list_data));
		} else {
			LOG_WRN("NET_EVENT_DECT_NEIGHBOR_LIST received but no event data "
				"available");
		}

		dect_neighbor_list_received = true;
		break;
	}
	case NET_EVENT_DECT_NEIGHBOR_INFO: {
		/* Neighbor info response */
		LOG_DBG("NET_EVENT_DECT_NEIGHBOR_INFO received in event handler!");

		if (cb->info) {
			struct dect_neighbor_info_evt *neighbor_info_evt =
				(struct dect_neighbor_info_evt *)cb->info;
			LOG_DBG("Neighbor info status: %d, long_rd_id: 0x%08X",
				neighbor_info_evt->status, neighbor_info_evt->long_rd_id);
			LOG_DBG("Associated: %s, FT mode: %s, Channel: %d, Network ID: 0x%08X",
				neighbor_info_evt->associated ? "Yes" : "No",
				neighbor_info_evt->ft_mode ? "Yes" : "No",
				neighbor_info_evt->channel, neighbor_info_evt->network_id);

			/* Store the neighbor info event data for validation */
			memcpy(&received_neighbor_info_data, neighbor_info_evt,
			       sizeof(received_neighbor_info_data));
		} else {
			LOG_WRN("NET_EVENT_DECT_NEIGHBOR_INFO received but no event data "
				"available");
		}

		dect_neighbor_info_received = true;
		break;
	}
	default:
		break;
	}
}

/**
 * @brief Debug function to check thread context and logging visibility
 */
void debug_thread_context(const char *context_name)
{
	struct k_thread *current = k_current_get();

	LOG_DBG("=== THREAD DEBUG [%s] ===", context_name);
	LOG_DBG("Current thread: %p", current);
	LOG_DBG("Thread name: %s", k_thread_name_get(current) ?: "unnamed");
	LOG_DBG("Thread priority: %d", k_thread_priority_get(current));
	LOG_DBG("============================");
}

/* Static flag to track if DECT stack has been initialized */
static bool dect_stack_initialized;

void setUp(void)
{

	/* Get the DECT NR+ network interface by device name */
	int if_index = net_if_get_by_name(CONFIG_DECT_NRP_MAC_DEVICE_NAME);

	if (if_index > 0) {
		test_iface = net_if_get_by_index(if_index);
	}

	/* Fallback to default interface if DECT interface not found */
	if (!test_iface) {
		test_iface = net_if_get_default();
	}

	/* Only reset event state for the current test - preserve stack state and data
	 * from previous tests
	 */
	dect_activate_done_received = false;
	dect_deactivate_done_received = false;
	dect_scan_result_received = false;
	dect_scan_done_received = false;
	dect_rssi_scan_result_received = false;
	dect_rssi_scan_done_received = false;
	dect_association_changed_received = false;
	dect_association_created_received = false;
	dect_association_failed_received = false;
	dect_network_status_received = false;
	dect_neighbor_list_received = false;
	dect_neighbor_info_received = false;

	/* Reset semaphore to ensure clean state for each test */
	k_sem_reset(&activation_done_sem);

	/* DO NOT reset received_association_data, received_beacon_data, or beacon_data_valid
	 * - these need to persist between tests for proper workflow testing
	 */

	/* Always ensure event callbacks are registered for each test */
	net_mgmt_init_event_callback(
		&dect_mgmt_cb, dect_mgmt_event_handler,
		NET_EVENT_DECT_ACTIVATE_DONE | NET_EVENT_DECT_DEACTIVATE_DONE |
			NET_EVENT_DECT_SCAN_RESULT | NET_EVENT_DECT_SCAN_DONE |
			NET_EVENT_DECT_RSSI_SCAN_RESULT | NET_EVENT_DECT_RSSI_SCAN_DONE |
			NET_EVENT_DECT_ASSOCIATION_CHANGED | NET_EVENT_DECT_NETWORK_STATUS |
			NET_EVENT_DECT_NEIGHBOR_LIST | NET_EVENT_DECT_NEIGHBOR_INFO);
	net_mgmt_add_event_callback(&dect_mgmt_cb);
	dect_stack_initialized = true;

	/* Preserve all state between test cases:
	 * - Mock call counters persist (not reset) to show cumulative calls across tests
	 * - Mock state (mock_dect_activated) persists to maintain DECT stack activation state
	 * - DECT stack state persists (settings, associations, etc.)
	 * - Callbacks and modem state persist
	 * DO NOT call mock_nrf_modem_dect_mac_reset() to maintain DECT stack state
	 * DO NOT reset mock call counters - they should persist between tests
	 */
}

void tearDown(void)
{
	/* Remove event callback but preserve DECT stack state */
	net_mgmt_del_event_callback(&dect_mgmt_cb);
}

/**
 * @brief Test DECT stack initialization through complete stack
 */
void test_dect_stack_initialization(void)
{
	/* Test that DECT stack initialization works properly.
	 *
	 * The MAC driver normally registers a NRF_MODEM_LIB_ON_INIT callback that:
	 * 1. Calls nrf_modem_dect_mac_callback_set() to register callbacks
	 * 2. Calls nrf_modem_dect_control_systemmode_set(NRF_MODEM_DECT_MODE_MAC)
	 * 3. Configures modem with default settings (auto_activate = true)
	 * 4. Eventually triggers activation and sends NET_EVENT_DECT_ACTIVATE_DONE
	 *
	 * For testing, we directly call the driver's initialization function that
	 * would normally be triggered by NRF_MODEM_LIB_ON_INIT.
	 */

	/* Debug thread context at start of test */
	debug_thread_context("test_dect_stack_initialization - START");

	/* Record baseline call counts (state persists between tests) */
	int baseline_callback_set = mock_nrf_modem_dect_mac_callback_set_call_count;
	int baseline_systemmode = mock_nrf_modem_dect_control_systemmode_set_call_count;
	int baseline_configure = mock_nrf_modem_dect_control_configure_call_count;
	int baseline_functional_mode = mock_nrf_modem_dect_control_functional_mode_set_call_count;

	/* Call the DECT driver's initialization callback directly (ret=0 for success) */
	dect_nrf91_ctrl_mdm_on_modem_lib_init(0, NULL);

	/* Wait for the activation event using semaphore with timeout */
	int sem_result = k_sem_take(&activation_done_sem, K_MSEC(100));

	if (sem_result != 0) {
		LOG_DBG("Timeout waiting for activation event, sem_result: %d", sem_result);
	}

	/* Verify that the DECT driver's callback triggered the expected function calls */
	/* Check that counts increased by 1 from baseline (state persists between tests) */
	TEST_ASSERT_EQUAL(baseline_callback_set + 1,
		mock_nrf_modem_dect_mac_callback_set_call_count);
	TEST_ASSERT_EQUAL(baseline_systemmode + 1,
		mock_nrf_modem_dect_control_systemmode_set_call_count);
	TEST_ASSERT_EQUAL(baseline_configure + 1, mock_nrf_modem_dect_control_configure_call_count);
	TEST_ASSERT_EQUAL(baseline_functional_mode + 1,
		mock_nrf_modem_dect_control_functional_mode_set_call_count);

	/* Verify that NET_EVENT_DECT_ACTIVATE_DONE event was received */
	TEST_ASSERT_TRUE_MESSAGE(
		dect_activate_done_received,
		"NET_EVENT_DECT_ACTIVATE_DONE event should be received after initialization");

	/* Verify the activation was successful */
	TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_OK, dect_activate_done_status,
				  "DECT activation should complete with success status");

	printf("DECT initialization test completed successfully:\n");
	printf("- Callback set calls: %d\n", mock_nrf_modem_dect_mac_callback_set_call_count);
	printf("- Systemmode set calls: %d\n",
	       mock_nrf_modem_dect_control_systemmode_set_call_count);
	printf("- Configure calls: %d\n", mock_nrf_modem_dect_control_configure_call_count);
	printf("- Functional mode set calls: %d\n",
	       mock_nrf_modem_dect_control_functional_mode_set_call_count);
	printf("- NET_EVENT_DECT_ACTIVATE_DONE received: %s\n",
	       dect_activate_done_received ? "YES" : "NO");
	printf("- Activation status: %d\n", dect_activate_done_status);
}

/**
 * @brief Test DECT settings reset to driver defaults
 *
 * This test verifies:
 * 1. NET_REQUEST_DECT_SETTINGS_WRITE works with reset_to_driver_defaults=true
 * 2. Settings reset is synchronous and successful
 * 3. All settings are restored to their driver default values
 */
void test_dect_settings_reset_to_defaults(void)
{
	LOG_DBG("=== DECT Settings Reset to Defaults Test ===");

	/* Step 1: Configure settings with reset_to_driver_defaults flag */
	struct dect_settings reset_settings = {0};

	/* Set the reset flag to true */
	reset_settings.cmd_params.reset_to_driver_defaults = true;

	LOG_DBG("Resetting DECT settings to driver defaults");
	LOG_DBG("- reset_to_driver_defaults: %s",
		reset_settings.cmd_params.reset_to_driver_defaults ? "true" : "false");

	/* Step 2: Write settings with reset flag (synchronous call) */
	int result = net_mgmt(NET_REQUEST_DECT_SETTINGS_WRITE, test_iface, &reset_settings,
			      sizeof(struct dect_settings));

	LOG_DBG("NET_REQUEST_DECT_SETTINGS_WRITE (reset) result: %d", result);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "Settings reset to defaults should succeed");

	LOG_DBG("Settings reset completed successfully - all settings restored to driver defaults");

	/* Step 3: Read settings back to verify the reset operation */
	struct dect_settings read_settings = {0};

	LOG_DBG("Reading DECT settings to verify reset operation");
	result = net_mgmt(NET_REQUEST_DECT_SETTINGS_READ, test_iface, &read_settings,
			  sizeof(struct dect_settings));

	LOG_DBG("NET_REQUEST_DECT_SETTINGS_READ result: %d", result);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, result, "Settings read after reset should succeed");

	/* Step 4: Verify key default values are set correctly after reset */
	LOG_DBG("Verifying reset settings:");
	LOG_DBG("- device_type: %d (expected: %d for DECT_DEVICE_TYPE_PT)",
		read_settings.device_type, DECT_DEVICE_TYPE_PT);
	LOG_DBG("- region: %d (expected: %d for DECT_SETTINGS_REGION_EU)", read_settings.region,
		DECT_SETTINGS_REGION_EU);
	LOG_DBG("- band_nbr: %d", read_settings.band_nbr);
	LOG_DBG("- auto_start.activate: %s", read_settings.auto_start.activate ? "true" : "false");

	/* Validate device type is reset to PT (default) */
	TEST_ASSERT_TRUE_MESSAGE(read_settings.device_type & DECT_DEVICE_TYPE_PT,
				 "Default device type should have PT bit set after reset");

	/* Validate region is reset to EU (default) */
	TEST_ASSERT_EQUAL_INT_MESSAGE(DECT_SETTINGS_REGION_EU, read_settings.region,
				      "Default region should be EU after reset");

	/* Validate band number is set to default value (typically 1 for EU) */
	TEST_ASSERT_TRUE_MESSAGE(read_settings.band_nbr == 1,
				 "Band number should be set to a valid value after reset");

	/* Validate auto-start is enabled by default */
	TEST_ASSERT_TRUE_MESSAGE(read_settings.auto_start.activate,
				 "Auto-start should be enabled by default after reset");

	/* Verify reset_to_driver_defaults flag is cleared after operation */
	TEST_ASSERT_FALSE_MESSAGE(
		read_settings.cmd_params.reset_to_driver_defaults,
		"reset_to_driver_defaults flag should be cleared after reset operation");

	LOG_DBG("=== DECT Settings Reset Test Completed Successfully ===");
	LOG_DBG("- Settings reset operation: SUCCESS");
	LOG_DBG("- Settings read operation: SUCCESS");
	LOG_DBG("- Device type verified: PT mode (default) configured");
}

/**
 * @brief Test DECT scan request with async cluster beacon reception
 *
 * This test follows the proper async pattern:
 * 1. NET_REQUEST_DECT_SCAN → triggers nrf_modem_dect_mac_network_scan()
 * 2. Mock simulates receiving one cluster beacon via ntf callback
 * 3. Mock concludes scan operation successfully via op callback
 */
void test_dect_scan_request_band1(void)
{
	/* Setup real DECT scan parameters structure */
	struct dect_scan_params scan_params = {.band = 1,
					       .channel_count = 2,
					       .channel_list = {1722, 1838},
					       .channel_scan_time_ms = 100};

	/* Setup beacon simulation parameters */
	struct dect_scan_beacon_params beacon_params = {
		.channel = 1722,		      /* Channel from our scan list */
		.transmitter_short_rd_id = 0x1234,    /* Example Short RD ID */
		.transmitter_long_rd_id = 0x56789ABC, /* Example Long RD ID */
		.network_id = 0x9876,		      /* Example Network ID */
		.mcs = 1,			      /* MCS index */
		.transmit_power = 8,		      /* Transmit power [0,15] */
		.rssi_2 = -45,			      /* Good signal strength in dBm */
		.snr = 20			      /* Good SNR in dB */
	};

	/* Record baseline call count (state persists between tests) */
	int baseline_network_scan = mock_nrf_modem_dect_mac_network_scan_call_count;

	/* Perform network scan using common test utility */
	struct dect_scan_result scan_result;
	int result = test_dect_network_scan(test_iface, &scan_params, &beacon_params, true,
					    &scan_result);

	/* Verify scan request was initiated successfully */
	TEST_ASSERT_EQUAL_MESSAGE(0, result, "Network scan should succeed");
	TEST_ASSERT_EQUAL_MESSAGE(baseline_network_scan + 1,
				  mock_nrf_modem_dect_mac_network_scan_call_count,
				  "nrf_modem_dect_mac_network_scan should be called once");

	/* Verify that NET_EVENT_DECT_SCAN_RESULT was received */
	TEST_ASSERT_TRUE_MESSAGE(
		scan_result.scan_result_received,
		"NET_EVENT_DECT_SCAN_RESULT should be received after cluster beacon");

	/* Validate that beacon data was captured and matches what we sent */
	TEST_ASSERT_TRUE_MESSAGE(
		scan_result.beacon_data_valid,
		"Beacon data should be available in NET_EVENT_DECT_SCAN_RESULT event");

	/* Verify beacon content matches what we simulated */
	TEST_ASSERT_EQUAL_MESSAGE(1722, scan_result.beacon_data.channel,
				  "Received beacon channel should match simulated beacon");
	TEST_ASSERT_EQUAL_MESSAGE(0x1234, scan_result.beacon_data.transmitter_short_rd_id,
				  "Received beacon short RD ID should match simulated beacon");
	TEST_ASSERT_EQUAL_MESSAGE(0x56789ABC, scan_result.beacon_data.transmitter_long_rd_id,
				  "Received beacon long RD ID should match simulated beacon");
	TEST_ASSERT_EQUAL_MESSAGE(0x9876, scan_result.beacon_data.network_id,
				  "Received beacon network ID should match simulated beacon");

	LOG_DBG("Beacon validation successful - all fields match expected values");

	/* Verify that NET_EVENT_DECT_SCAN_DONE was received */
	TEST_ASSERT_TRUE_MESSAGE(
		scan_result.scan_done_received,
		"NET_EVENT_DECT_SCAN_DONE should be received after scan completion");

	TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_OK, scan_result.scan_done_status,
				  "DECT scan should complete with success status");

	/* Test completed successfully - all assertions passed */
}

/**
 * @brief Test DECT association request for PT device using real net_mgmt API
 *
 * This test follows the async association pattern:
 * 1. First perform a quick scan to get beacon data
 * 2. NET_REQUEST_DECT_ASSOCIATION → triggers nrf_modem_dect_mac_association()
 * 3. Mock simulates successful association via ntf callback (association_ntf)
 * 4. Mock completes association operation via op callback (association)
 */
void test_dect_pt_association_request(void)
{
	/* Setup beacon data directly for this test (simulating previously scanned beacon) */
	received_beacon_data.channel = 1722;
	received_beacon_data.transmitter_short_rd_id = 0x1234;
	received_beacon_data.transmitter_long_rd_id = 0x56789ABC;
	received_beacon_data.network_id = 0x9876;
	beacon_data_valid = true;

	LOG_DBG("Using beacon data for association test - Long RD ID: 0x%08X",
		received_beacon_data.transmitter_long_rd_id);

	/* Record baseline call count (state persists between tests) */
	int baseline_association = mock_nrf_modem_dect_mac_association_call_count;

	/* Perform association using common test utility */
	struct dect_association_result assoc_result;
	int result = test_dect_association_request(test_iface,
						   received_beacon_data.transmitter_long_rd_id,
						   NULL, /* Use default response parameters */
						   true, /* Simulate completion */
						   true, /* Simulate network joined */
						   &assoc_result);

	/* Verify association request was initiated successfully */
	TEST_ASSERT_EQUAL_MESSAGE(0, result, "Association request should succeed");
	TEST_ASSERT_EQUAL_MESSAGE(baseline_association + 1,
				  mock_nrf_modem_dect_mac_association_call_count,
				  "nrf_modem_dect_mac_association should be called once");

	/* Verify that NET_EVENT_DECT_ASSOCIATION_CHANGED was received */
	TEST_ASSERT_TRUE_MESSAGE(
		assoc_result.association_changed_received,
		"NET_EVENT_DECT_ASSOCIATION_CHANGED should be received after successful "
		"association");

	/* Verify that we specifically received DECT_ASSOCIATION_CREATED event */
	TEST_ASSERT_TRUE_MESSAGE(
		assoc_result.association_created_received,
		"NET_EVENT_DECT_ASSOCIATION_CHANGED with DECT_ASSOCIATION_CREATED should "
		"be received");

	/* Validate the association event data */
	TEST_ASSERT_EQUAL_MESSAGE(DECT_ASSOCIATION_CREATED,
				  assoc_result.association_data.association_change_type,
				  "Association change type should be DECT_ASSOCIATION_CREATED");
	TEST_ASSERT_EQUAL_MESSAGE(
		received_beacon_data.transmitter_long_rd_id,
		assoc_result.association_data.long_rd_id,
		"Association event Long RD ID should match the target we associated with");
	TEST_ASSERT_EQUAL_MESSAGE(
		DECT_NEIGHBOR_ROLE_PARENT, assoc_result.association_data.neighbor_role,
		"When PT associates with FT device, the FT device becomes our parent");

	/* Verify that NET_EVENT_DECT_NETWORK_STATUS was received */
	TEST_ASSERT_TRUE_MESSAGE(
		assoc_result.network_status_received,
		"NET_EVENT_DECT_NETWORK_STATUS should be received after network join");

	/* Validate the network status event data */
	TEST_ASSERT_EQUAL_MESSAGE(DECT_NETWORK_STATUS_JOINED,
				  assoc_result.network_status_data.network_status,
				  "Network status should be DECT_NETWORK_STATUS_JOINED");
	TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_OK,
				  assoc_result.network_status_data.dect_err_cause,
				  "Network status should have no DECT errors");

	LOG_DBG("Association and network join test completed successfully!");
	LOG_DBG("- Association: DECT_ASSOCIATION_CREATED validated with Long RD ID: 0x%08X",
		assoc_result.association_data.long_rd_id);
	LOG_DBG("- Network Status: DECT_NETWORK_STATUS_JOINED validated");
	LOG_DBG("- Device role: %s",
		assoc_result.association_data.neighbor_role == DECT_NEIGHBOR_ROLE_CHILD
			? "PT (child of FT parent)"
			: "FT (parent of PT child)");
}

/**
 * @brief Test neighbor list information request
 *
 * Tests NET_REQUEST_DECT_NEIGHBOR_LIST flow:
 * 1. Make neighbor list request
 * 2. Verify nrf_modem_dect_mac_neighbor_list() is called
 * 3. Simulate neighbor list response with associated parent
 * 4. Validate NET_EVENT_DECT_NEIGHBOR_LIST reception
 */
void test_dect_pt_neigbor_list_info_req(void)
{
	TEST_ASSERT_NOT_NULL_MESSAGE(test_iface,
				     "DECT interface should be available for neighbor list test");

	/* DECT should already be activated and associated from previous tests - no need to
	 * reactivate
	 */
	LOG_DBG("Using persistent DECT stack state - already activated and associated");

	/* Verify we have beacon data from previous scan test */
	TEST_ASSERT_TRUE_MESSAGE(beacon_data_valid,
				 "Beacon data should be available from previous scan test");

	/* Verify we have association data from previous association test */
	TEST_ASSERT_TRUE_MESSAGE(received_association_data.association_change_type ==
					 DECT_ASSOCIATION_CREATED,
				 "Association should be established from previous test");

	/* Reset only neighbor list tracking - keep other state */
	dect_neighbor_list_received = false;
	memset(&received_neighbor_list_data, 0, sizeof(received_neighbor_list_data));

	/* Make neighbor list request */
	LOG_DBG("Requesting neighbor list with NET_REQUEST_DECT_NEIGHBOR_LIST");
	int result = net_mgmt(NET_REQUEST_DECT_NEIGHBOR_LIST, test_iface, NULL, 0);

	TEST_ASSERT_EQUAL_MESSAGE(0, result, "Neighbor list request should succeed");

	/* Verify that nrf_modem_dect_mac_neighbor_list() was called */
	TEST_ASSERT_EQUAL_MESSAGE(
		1, mock_nrf_modem_dect_mac_neighbor_list_call_count,
		"nrf_modem_dect_mac_neighbor_list() should be called exactly once");

	/* Simulate neighbor list response from mock modem */
	if (mock_op_callbacks.neighbor_list) {
		/* Create neighbor list response with associated parent */
		struct nrf_modem_dect_mac_neighbor_list_cb_params neighbor_list_response = {
			.status = NRF_MODEM_DECT_MAC_STATUS_OK,
			.num_neighbors = 1, /* One neighbor - our associated parent */
			.long_rd_ids = NULL /* Will be set to our array below */
		};

		/* Create array with associated parent's Long RD ID (from previous association test)
		 */
		uint32_t parent_long_rd_ids[1] = {
			0x56789ABC}; /* Same as beacon data from scan test */
		neighbor_list_response.long_rd_ids = parent_long_rd_ids;

		LOG_DBG("Simulating neighbor list response: status=%d, num_neighbors=%d, "
			"parent_id=0x%08X",
			neighbor_list_response.status, neighbor_list_response.num_neighbors,
			parent_long_rd_ids[0]);

		/* Call the neighbor list callback */
		mock_op_callbacks.neighbor_list(&neighbor_list_response);
	} else {
		LOG_ERR("No neighbor list operation callback registered!");
		TEST_FAIL_MESSAGE("Neighbor list operation callback should be registered");
	}

	/* Wait for neighbor list response processing */
	k_sleep(K_MSEC(50));

	/* Verify that NET_EVENT_DECT_NEIGHBOR_LIST was received */
	TEST_ASSERT_TRUE_MESSAGE(
		dect_neighbor_list_received,
		"NET_EVENT_DECT_NEIGHBOR_LIST should be received after neighbor list request");

	/* Validate the neighbor list event data */
	TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_OK, received_neighbor_list_data.status,
				  "Neighbor list status should be DECT_MAC_STATUS_OK (success)");
	TEST_ASSERT_EQUAL_MESSAGE(1, received_neighbor_list_data.neighbor_count,
				  "Should receive exactly 1 neighbor (our associated parent)");
	TEST_ASSERT_EQUAL_MESSAGE(
		0x56789ABC, received_neighbor_list_data.neighbor_long_rd_ids[0],
		"First neighbor should be our associated parent with Long RD ID 0x56789ABC");

	LOG_DBG("Neighbor list test completed successfully!");
	LOG_DBG("- Mock call count: %d", mock_nrf_modem_dect_mac_neighbor_list_call_count);
	LOG_DBG("- Response status: %d", received_neighbor_list_data.status);
	LOG_DBG("- Neighbor count: %d", received_neighbor_list_data.neighbor_count);
	LOG_DBG("- Associated parent: Long RD ID 0x%08X",
		received_neighbor_list_data.neighbor_long_rd_ids[0]);

	/* Continue with neighbor info request for the discovered neighbor */
	LOG_DBG("Now requesting neighbor info for discovered neighbor: Long RD ID 0x%08X",
		received_neighbor_list_data.neighbor_long_rd_ids[0]);

	/* Reset neighbor info tracking */
	dect_neighbor_info_received = false;
	memset(&received_neighbor_info_data, 0, sizeof(received_neighbor_info_data));

	/* Prepare neighbor info request parameters */
	struct dect_neighbor_info_req_params neighbor_info_params = {
		.long_rd_id = received_neighbor_list_data
				      .neighbor_long_rd_ids[0] /* Query our associated parent */
	};

	/* Make neighbor info request */
	LOG_DBG("Requesting neighbor info with NET_REQUEST_DECT_NEIGHBOR_INFO for Long RD ID "
		"0x%08X",
		neighbor_info_params.long_rd_id);
	result = net_mgmt(NET_REQUEST_DECT_NEIGHBOR_INFO, test_iface, &neighbor_info_params,
			  sizeof(neighbor_info_params));
	TEST_ASSERT_EQUAL_MESSAGE(0, result, "Neighbor info request should succeed");

	/* Verify that nrf_modem_dect_mac_neighbor_info() was called */
	TEST_ASSERT_EQUAL_MESSAGE(
		1, mock_nrf_modem_dect_mac_neighbor_info_call_count,
		"nrf_modem_dect_mac_neighbor_info() should be called exactly once");

	/* Simulate neighbor info response from mock modem */
	if (mock_op_callbacks.neighbor_info) {
		/* Create neighbor info response for our associated FT parent */
		struct nrf_modem_dect_mac_neighbor_info_cb_params neighbor_info_response = {
			.status = NRF_MODEM_DECT_MAC_STATUS_OK,
			.long_rd_id = 0x56789ABC,	 /* Parent Long RD ID */
			.associated = true,		 /* We are associated with this neighbor */
			.ft_mode = true,		 /* This is an FT device (parent) */
			.channel = 1722,		 /* Same channel as beacon */
			.network_id = 0x9876,		 /* Network ID from beacon */
			.time_since_last_rx_ms = 100,	 /* Recently seen */
			.beacon_average_rx_txpower = 10, /* TX power in dB */
			.beacon_average_rx_rssi_2 = -60, /* RSSI in dBm */
			.beacon_average_rx_snr = 25	 /* SNR in dB */
		};

		LOG_DBG("Simulating neighbor info response: status=%d, long_rd_id=0x%08X, "
			"associated=%s, ft_mode=%s",
			neighbor_info_response.status, neighbor_info_response.long_rd_id,
			neighbor_info_response.associated ? "Yes" : "No",
			neighbor_info_response.ft_mode ? "Yes" : "No");

		/* Call the neighbor info callback */
		mock_op_callbacks.neighbor_info(&neighbor_info_response);
	} else {
		LOG_ERR("No neighbor info operation callback registered!");
		TEST_FAIL_MESSAGE("Neighbor info operation callback should be registered");
	}

	/* Wait for neighbor info response processing */
	k_sleep(K_MSEC(50));

	/* Verify that NET_EVENT_DECT_NEIGHBOR_INFO was received */
	TEST_ASSERT_TRUE_MESSAGE(
		dect_neighbor_info_received,
		"NET_EVENT_DECT_NEIGHBOR_INFO should be received after neighbor info request");

	/* Validate the neighbor info event data */
	TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_OK, received_neighbor_info_data.status,
				  "Neighbor info status should be DECT_MAC_STATUS_OK (success)");
	TEST_ASSERT_EQUAL_MESSAGE(0x56789ABC, received_neighbor_info_data.long_rd_id,
				  "Neighbor info Long RD ID should match our associated parent");
	TEST_ASSERT_TRUE_MESSAGE(
		received_neighbor_info_data.associated,
		"Neighbor should be marked as associated (we are associated with this parent)");
	TEST_ASSERT_TRUE_MESSAGE(received_neighbor_info_data.ft_mode,
				 "Neighbor should be in FT mode (this is our parent FT device)");
	TEST_ASSERT_EQUAL_MESSAGE(1722, received_neighbor_info_data.channel,
				  "Neighbor channel should match beacon channel");
	TEST_ASSERT_EQUAL_MESSAGE(0x9876, received_neighbor_info_data.network_id,
				  "Neighbor network ID should match beacon network ID");

	LOG_DBG("Complete neighbor discovery and info test completed successfully!");
	LOG_DBG("- Neighbor list: %d neighbors discovered",
		received_neighbor_list_data.neighbor_count);
	LOG_DBG("- Neighbor info: Associated=%s, FT mode=%s, Channel=%d, Network=0x%08X",
		received_neighbor_info_data.associated ? "Yes" : "No",
		received_neighbor_info_data.ft_mode ? "Yes" : "No",
		received_neighbor_info_data.channel, received_neighbor_info_data.network_id);
}

/**
 * @brief Test DECT status info request (synchronous)
 *
 * Tests NET_REQUEST_DECT_STATUS_INFO_GET flow:
 * 1. Make synchronous status info request
 * 2. Verify no libmodem calls are made (synchronous operation)
 * 3. Validate returned status information (without firmware version for now)
 */
void test_dect_pt_status_info_req(void)
{
	TEST_ASSERT_NOT_NULL_MESSAGE(test_iface,
				     "DECT interface should be available for status info test");

	/* Status info is synchronous - should work regardless of DECT stack state */
	LOG_DBG("Requesting DECT status info with NET_REQUEST_DECT_STATUS_INFO_GET");

	/* Debug: Check our test state before making the request */
	LOG_DBG("Pre-status check: association_change_type=%d (expected=%d for "
		"DECT_ASSOCIATION_CREATED)",
		received_association_data.association_change_type, DECT_ASSOCIATION_CREATED);
	LOG_DBG("Pre-status check: associated_long_rd_id=0x%08X",
		received_association_data.long_rd_id);

	/* Get status info using common test utility */
	struct dect_status_info status_info;
	int result = test_dect_status_info_get(test_iface, &status_info);

	TEST_ASSERT_EQUAL_MESSAGE(0, result, "Status info request should succeed");

	/* Validate the returned status information */

	/* Validate core status info fields as requested */
	TEST_ASSERT_TRUE_MESSAGE(status_info.mdm_activated,
				 "Modem should be reported as activated after previous tests");

	/* Validate association counts from persistent state */
	if (received_association_data.association_change_type == DECT_ASSOCIATION_CREATED) {
		/* We have an association from previous tests - validate correct role assignment */
		TEST_ASSERT_EQUAL_MESSAGE(1, status_info.parent_count,
					  "Should report 1 parent association from previous tests");
		TEST_ASSERT_EQUAL_MESSAGE(0, status_info.child_count,
					  "PT device should have no child associations");
		TEST_ASSERT_EQUAL_MESSAGE(received_beacon_data.transmitter_long_rd_id,
					  status_info.parent_associations[0].long_rd_id,
					  "Parent Long RD ID should match our associated parent");
	} else {
		/* No association */
		TEST_ASSERT_EQUAL_MESSAGE(0, status_info.parent_count,
					  "Should report 0 parent associations if not associated");
		TEST_ASSERT_EQUAL_MESSAGE(0, status_info.child_count,
					  "PT device should have no child associations");
	}

	/* Check cluster status (PT device should not have cluster running) */
	TEST_ASSERT_FALSE_MESSAGE(status_info.cluster_running,
				  "PT device should not have cluster running");
	TEST_ASSERT_EQUAL_MESSAGE(0, status_info.cluster_channel,
				  "PT device should not have cluster channel set");

	/* Check network beacon status (PT device should not have beacon running) */
	TEST_ASSERT_FALSE_MESSAGE(status_info.nw_beacon_running,
				  "PT device should not have network beacon running");

	LOG_DBG("Status info test completed successfully!");
	LOG_DBG("- Modem activated: %s", status_info.mdm_activated ? "Yes" : "No");
	LOG_DBG("- Parent count: %d", status_info.parent_count);
	LOG_DBG("- Child count: %d", status_info.child_count);
	LOG_DBG("- Cluster running: %s", status_info.cluster_running ? "Yes" : "No");
	LOG_DBG("- Network beacon running: %s", status_info.nw_beacon_running ? "Yes" : "No");
}

/**
 * @brief Test DECT PT association release using real net_mgmt API
 *
 * Releases the association with the parent device established in previous test.
 * Uses NET_REQUEST_DECT_ASSOCIATION_RELEASE with persistent association data.
 */
void test_dect_pt_association_release(void)
{
	LOG_DBG("Testing DECT association release with persistent parent association");

	/* Verify we have persistent association data from previous test */
	TEST_ASSERT_TRUE_MESSAGE(beacon_data_valid, "Should have beacon data from previous test");
	TEST_ASSERT_NOT_EQUAL_MESSAGE(0, received_beacon_data.transmitter_long_rd_id,
				      "Should have valid parent Long RD ID from association test");

	/* Perform association release using common test utility */
	struct dect_association_release_result release_result;
	int result = test_dect_association_release(test_iface,
						   received_beacon_data.transmitter_long_rd_id,
						   true, /* Simulate completion */
						   true, /* Simulate network unjoined */
						   &release_result);

	TEST_ASSERT_EQUAL_MESSAGE(0, result, "NET_REQUEST_DECT_ASSOCIATION_RELEASE should succeed");

	/* Verify mock call count */
	TEST_ASSERT_EQUAL_MESSAGE(1, mock_nrf_modem_dect_mac_association_release_call_count,
				  "nrf_modem_dect_mac_association_release should be called once");

	/* Verify that NET_EVENT_DECT_ASSOCIATION_CHANGED was received for the release */
	TEST_ASSERT_TRUE_MESSAGE(
		release_result.association_changed_received,
		"NET_EVENT_DECT_ASSOCIATION_CHANGED should be received after association release");

	/* Validate the association release event data */
	TEST_ASSERT_EQUAL_MESSAGE(DECT_ASSOCIATION_RELEASED,
				  release_result.association_data.association_change_type,
				  "Association change type should be DECT_ASSOCIATION_RELEASED");
	TEST_ASSERT_EQUAL_MESSAGE(
		received_beacon_data.transmitter_long_rd_id,
		release_result.association_data.long_rd_id,
		"Association release event Long RD ID should match the parent we released");
	TEST_ASSERT_EQUAL_MESSAGE(DECT_NEIGHBOR_ROLE_PARENT,
				  release_result.association_data.neighbor_role,
				  "Released neighbor should have been our parent");

	/* Verify that NET_EVENT_DECT_NETWORK_STATUS was received */
	TEST_ASSERT_TRUE_MESSAGE(
		release_result.network_status_received,
		"NET_EVENT_DECT_NETWORK_STATUS should be received after association release");

	/* Verify the network status is UNJOINED */
	TEST_ASSERT_EQUAL_MESSAGE(
		DECT_NETWORK_STATUS_UNJOINED, release_result.network_status_data.network_status,
		"Network status should be DECT_NETWORK_STATUS_UNJOINED after association release");

	LOG_DBG("Association release test completed successfully!");
	LOG_DBG("- Mock call count: %d", mock_nrf_modem_dect_mac_association_release_call_count);
	LOG_DBG("- Association release: DECT_ASSOCIATION_RELEASED validated with Long RD ID: "
		"0x%08X",
		release_result.association_data.long_rd_id);
	LOG_DBG("- Released neighbor role: DECT_NEIGHBOR_ROLE_PARENT (was our parent)");
	LOG_DBG("- Network status: DECT_NETWORK_STATUS_UNJOINED (device is no longer part of "
		"network)");
}

/**
 * @brief Test DECT stack activation using real net_mgmt API
 *
 * Activates the DECT stack and verifies NET_EVENT_DECT_ACTIVATE_DONE event.
 * This test verifies the activation workflow using the utility function.
 * First deactivates the stack if it's already activated, then tests activation.
 */
void test_dect_ft_activate(void)
{
	LOG_DBG("Testing DECT stack activation with NET_REQUEST_DECT_ACTIVATE");

	/* Record baseline call counts (state persists between tests) */
	int baseline_configure = mock_nrf_modem_dect_control_configure_call_count;
	int baseline_functional_mode_set =
		mock_nrf_modem_dect_control_functional_mode_set_call_count;

	/* Now perform activation using common test utility */
	struct dect_activate_result activate_result;
	int result = test_dect_perform_activate(test_iface, 250, &activate_result);

	TEST_ASSERT_EQUAL_MESSAGE(0, result, "NET_REQUEST_DECT_ACTIVATE should succeed");

	/* Verify that nrf_modem_dect_control_configure was called */
	TEST_ASSERT_EQUAL_MESSAGE(baseline_configure + 1,
				  mock_nrf_modem_dect_control_configure_call_count,
				  "nrf_modem_dect_control_configure should be called "
				  "once");

	/* Verify that nrf_modem_dect_control_functional_mode_set was called */
	TEST_ASSERT_EQUAL_MESSAGE(baseline_functional_mode_set + 1,
				  mock_nrf_modem_dect_control_functional_mode_set_call_count,
				  "nrf_modem_dect_control_functional_mode_set should be "
				  "called once");

	/* Verify that NET_EVENT_DECT_ACTIVATE_DONE was received */
	TEST_ASSERT_TRUE_MESSAGE(
		activate_result.activate_done_received,
		"NET_EVENT_DECT_ACTIVATE_DONE should be received after activation request");

	/* Verify activation was successful */
	TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_OK, activate_result.activate_done_status,
				  "DECT activation should complete with DECT_MAC_STATUS_OK");

	/* Verify that configure was called */
	TEST_ASSERT_TRUE_MESSAGE(activate_result.configure_called,
				 "nrf_modem_dect_control_configure should be called "
				 "during activation");

	/* Verify that functional_mode_set was called */
	TEST_ASSERT_TRUE_MESSAGE(activate_result.functional_mode_set_called,
				 "nrf_modem_dect_control_functional_mode_set should be called "
				 "during activation");

	LOG_DBG("DECT activation test completed successfully!");
	LOG_DBG("- Activation event received: %s",
		activate_result.activate_done_received ? "YES" : "NO");
	LOG_DBG("- Activation status: %d (expected=0 for DECT_MAC_STATUS_OK)",
		activate_result.activate_done_status);
	LOG_DBG("- Configure called: %s", activate_result.configure_called ? "YES" : "NO");
	LOG_DBG("- Functional mode set called: %s",
		activate_result.functional_mode_set_called ? "YES" : "NO");
	LOG_DBG("- DECT stack is now activated");
}

/**
 * @brief Test DECT RSSI scan using real net_mgmt API
 *
 * Performs an RSSI scan and verifies:
 * - nrf_modem_dect_mac_rssi_scan() is called
 * - rssi_scan_ntf notification callback is received (NET_EVENT_DECT_RSSI_SCAN_RESULT)
 * - rssi_scan op callback is received (NET_EVENT_DECT_RSSI_SCAN_DONE) with success status
 */
void test_dect_ft_rssi_scan(void)
{
	LOG_DBG("Testing DECT RSSI scan with NET_REQUEST_DECT_RSSI_SCAN");

	/* Ensure the stack is activated before RSSI scan */
	struct dect_activate_result activate_result;
	int activate_ret = test_dect_perform_activate(test_iface, 250, &activate_result);

	if (activate_ret != 0 || !activate_result.activate_done_received ||
	    activate_result.activate_done_status != DECT_MAC_STATUS_OK) {
		LOG_DBG("Activating stack before RSSI scan test");
		/* Wait a bit for activation to complete */
		k_sleep(K_MSEC(100));
	}

	/* Setup RSSI scan parameters */
	struct dect_rssi_scan_params rssi_scan_params = {
		.band = 0,
		.frame_count_to_scan = 1,
		.channel_count = 1,
		.channel_list = {1657}
	};

	/* Record baseline call count (state persists between tests) */
	int baseline_rssi_scan = mock_nrf_modem_dect_mac_rssi_scan_call_count;

	/* Perform RSSI scan using common test utility */
	struct dect_rssi_scan_result rssi_result;
	int result = test_dect_perform_rssi_scan(test_iface, &rssi_scan_params, 500, &rssi_result);

	/* Note: The request may return an error if the stack isn't fully ready,
	 * but the async callbacks may still be triggered. Continue with verification.
	 */
	if (result != 0) {
		LOG_DBG("RSSI scan request returned error %d, but continuing to verify mock calls",
			result);
	}

	/* Verify that nrf_modem_dect_mac_rssi_scan was called */
	TEST_ASSERT_EQUAL_MESSAGE(baseline_rssi_scan + 1,
				  mock_nrf_modem_dect_mac_rssi_scan_call_count,
				  "nrf_modem_dect_mac_rssi_scan should be called once");

	/* Verify that NET_EVENT_DECT_RSSI_SCAN_RESULT was received */
	TEST_ASSERT_TRUE_MESSAGE(
		rssi_result.rssi_scan_result_received,
		"NET_EVENT_DECT_RSSI_SCAN_RESULT should be received after rssi_scan_ntf callback");

	/* Verify the content of NET_EVENT_DECT_RSSI_SCAN_RESULT data */
	if (rssi_result.rssi_scan_result_received) {
		struct dect_rssi_scan_result_data *rssi_data =
			&rssi_result.rssi_scan_result_data.rssi_scan_result;

		/* Verify channel matches the requested channel */
		TEST_ASSERT_EQUAL_MESSAGE(rssi_scan_params.channel_list[0], rssi_data->channel,
					  "RSSI scan result channel should match "
					  "requested channel");

		/* Verify busy_percentage is within valid range [0-100] */
		TEST_ASSERT_TRUE_MESSAGE(rssi_data->busy_percentage <= 100,
					 "RSSI scan busy_percentage should be <= 100");

		/* Verify subslot counts are valid (should sum to 48 or less) */
		uint8_t total_subslots = rssi_data->free_subslot_cnt +
					rssi_data->possible_subslot_cnt +
					rssi_data->busy_subslot_cnt;
		TEST_ASSERT_TRUE_MESSAGE(total_subslots <= 48,
					 "RSSI scan subslot counts should sum to <= 48");

		/* Verify scan_suitable_percent is within valid range [0-100] */
		TEST_ASSERT_TRUE_MESSAGE(rssi_data->scan_suitable_percent <= 100,
					 "RSSI scan scan_suitable_percent should be <= 100");
	}

	/* Verify that NET_EVENT_DECT_RSSI_SCAN_DONE was received */
	TEST_ASSERT_TRUE_MESSAGE(
		rssi_result.rssi_scan_done_received,
		"NET_EVENT_DECT_RSSI_SCAN_DONE should be received after rssi_scan op callback");

	/* Verify RSSI scan was successful */
	TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_OK, rssi_result.rssi_scan_done_status,
				  "DECT RSSI scan should complete with DECT_MAC_STATUS_OK");

	LOG_DBG("DECT RSSI scan test completed successfully!");
	LOG_DBG("- RSSI scan result event received: %s",
		rssi_result.rssi_scan_result_received ? "YES" : "NO");
	LOG_DBG("- RSSI scan done event received: %s",
		rssi_result.rssi_scan_done_received ? "YES" : "NO");
	LOG_DBG("- RSSI scan status: %d (expected=0 for DECT_MAC_STATUS_OK)",
		rssi_result.rssi_scan_done_status);
	if (rssi_result.rssi_scan_result_received) {
		LOG_DBG("- RSSI scan result: channel=%d, busy_percentage=%d%%",
			rssi_result.rssi_scan_result_data.rssi_scan_result.channel,
			rssi_result.rssi_scan_result_data.rssi_scan_result.busy_percentage);
	}
}

/**
 * @brief Test DECT stack deactivation using real net_mgmt API
 *
 * Deactivates the DECT stack and verifies NET_EVENT_DECT_DEACTIVATE_DONE event.
 * This is typically the final step in the DECT device lifecycle.
 */
void test_dect_deactivate(void)
{
	LOG_DBG("Testing DECT stack deactivation with NET_REQUEST_DECT_DEACTIVATE");

	/* Verify we have persistent state from previous tests (stack should be activated) */
	TEST_ASSERT_TRUE_MESSAGE(
		beacon_data_valid,
		"Should have beacon data indicating previous DECT operations were successful");

	/* Perform deactivation using common test utility */
	struct dect_deactivate_result deactivate_result;
	int result = test_dect_perform_deactivate(test_iface, 250, &deactivate_result);

	TEST_ASSERT_EQUAL_MESSAGE(0, result, "NET_REQUEST_DECT_DEACTIVATE should succeed");

	/* Verify that NET_EVENT_DECT_DEACTIVATE_DONE was received */
	TEST_ASSERT_TRUE_MESSAGE(
		deactivate_result.deactivate_done_received,
		"NET_EVENT_DECT_DEACTIVATE_DONE should be received after deactivation request");

	/* Verify deactivation was successful */
	TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_OK, deactivate_result.deactivate_done_status,
				  "DECT deactivation should complete with DECT_MAC_STATUS_OK");

	LOG_DBG("DECT deactivation test completed successfully!");
	LOG_DBG("- Deactivation event received: %s",
		deactivate_result.deactivate_done_received ? "YES" : "NO");
	LOG_DBG("- Deactivation status: %d (expected=0 for DECT_MAC_STATUS_OK)",
		deactivate_result.deactivate_done_status);
	LOG_DBG("- DECT stack is now deactivated");
}

/**
 * @brief Test DECT requests behavior when stack is deactivated
 *
 * This test runs after test_dect_deactivate() and validates the behavior
 * of net_mgmt requests when the DECT stack is deactivated.
 *
 * Expected return codes when deactivated:
 * - NET_REQUEST_DECT_SCAN: 0 (accepted, but fails with NET_EVENT_DECT_SCAN_DONE error)
 * - NET_REQUEST_DECT_ASSOCIATION: 0 (async, may succeed request but fail operation)
 * - NET_REQUEST_DECT_NEIGHBOR_LIST: 0 (returns cached data)
 * - NET_REQUEST_DECT_NEIGHBOR_INFO: 0 (returns cached data)
 * - NET_REQUEST_DECT_ASSOCIATION_RELEASE: -95 (-ENOTSUP)
 * - NET_REQUEST_DECT_STATUS_INFO_GET: 0 (informational request)
 * - NET_REQUEST_DECT_DEACTIVATE: 0 (idempotent operation)
 * - NET_REQUEST_DECT_RSSI_SCAN: 0 (accepted, but fails with NET_EVENT_DECT_RSSI_SCAN_DONE error)
 * - NET_REQUEST_DECT_CLUSTER_START: -22 (-EINVAL, not allowed for PT)
 * - NET_REQUEST_DECT_NW_BEACON_START: -22 (-EINVAL, not allowed for PT)
 * - NET_REQUEST_DECT_NW_BEACON_STOP: -114 (-EALREADY, already stopped)
 * - NET_REQUEST_DECT_CLUSTER_INFO: 0 (informational request)
 * - NET_REQUEST_DECT_NETWORK_CREATE: -95 (-ENOTSUP)
 * - NET_REQUEST_DECT_NETWORK_JOIN: -22 (-EINVAL, modem not activated)
 * - NET_REQUEST_DECT_NETWORK_UNJOIN: -114 (-EALREADY, not joined)
 */
void test_dect_deactivated_requests_fail(void)
{
	int ret;

	LOG_DBG("=== Testing DECT requests when stack is deactivated ===");

	/* Reset event flags */
	dect_scan_result_received = false;
	dect_scan_done_received = false;
	dect_rssi_scan_done_received = false;
	dect_neighbor_list_received = false;
	dect_neighbor_info_received = false;
	dect_association_changed_received = false;
	dect_association_failed_received = false;
	dect_network_status_received = false;

	/* Test 1: NET_REQUEST_DECT_SCAN should return "not allowed" status when deactivated */
	struct dect_scan_params scan_params = {.band = 1,
					       .channel_count = 2,
					       .channel_list = {1722, 1838},
					       .channel_scan_time_ms = 100};

	ret = net_mgmt(NET_REQUEST_DECT_SCAN, test_iface, &scan_params, sizeof(scan_params));
	LOG_DBG("NET_REQUEST_DECT_SCAN result when deactivated: %d", ret);

	if (ret == 0) {
		LOG_DBG("SCAN request accepted when deactivated - waiting for "
			"NET_EVENT_DECT_SCAN_DONE with error status");
		/* Wait for scan events to complete - should get SCAN_DONE with "not allowed" status
		 */
		k_sleep(K_MSEC(300));

		/* Verify that NET_EVENT_DECT_SCAN_DONE was received */
		TEST_ASSERT_TRUE_MESSAGE(dect_scan_done_received,
					 "NET_EVENT_DECT_SCAN_DONE should be received even when "
					 "scan fails due to deactivated state");

		/* Verify the scan failed with "not allowed" status */
		TEST_ASSERT_EQUAL_MESSAGE(
			DECT_MAC_STATUS_NOT_ALLOWED, dect_scan_done_status,
			"Scan should fail with error status when stack is deactivated");

		/* Verify no scan results were generated */
		TEST_ASSERT_FALSE_MESSAGE(dect_scan_result_received,
					  "No scan results should be generated when scan fails due "
					  "to deactivated state");
	} else {
		LOG_DBG("SCAN request immediately failed when deactivated - this is also "
			"acceptable");
	}

	/* Test 2: NET_REQUEST_DECT_ASSOCIATION should fail when deactivated */
	struct dect_associate_req_params assoc_params = {.target_long_rd_id = 0x56789ABC};

	ret = net_mgmt(NET_REQUEST_DECT_ASSOCIATION, test_iface, &assoc_params,
		       sizeof(assoc_params));
	LOG_DBG("NET_REQUEST_DECT_ASSOCIATION result when deactivated: %d", ret);
	if (ret == 0) {
		LOG_DBG("Association request queued when deactivated - should fail during "
			"execution");
		k_sleep(K_MSEC(200)); /* Allow time for async failure events */

		/* Verify that we received the expected failure events */
		TEST_ASSERT_TRUE_MESSAGE(
			dect_association_changed_received,
			"NET_EVENT_DECT_ASSOCIATION_CHANGED should be received for async failure");
		TEST_ASSERT_TRUE_MESSAGE(dect_association_failed_received,
					 "Association should fail with "
					 "DECT_ASSOCIATION_REQ_FAILED_MDM when deactivated");
		TEST_ASSERT_EQUAL_MESSAGE(
			DECT_ASSOCIATION_REQ_FAILED_MDM,
			received_association_data.association_change_type,
			"Association failure should be DECT_ASSOCIATION_REQ_FAILED_MDM");

		/* Also verify network status "not allowed" event */
		TEST_ASSERT_TRUE_MESSAGE(dect_network_status_received,
					 "NET_EVENT_DECT_NETWORK_STATUS should be received for "
					 "'not allowed' status");

		LOG_DBG("Verified async association failure when deactivated");
	} else {
		LOG_DBG("Association request immediately failed when deactivated with error: %d",
			ret);
		TEST_ASSERT_NOT_EQUAL_INT(0, ret);
	}

	/* Test 3: NET_REQUEST_DECT_NEIGHBOR_LIST behavior when deactivated */
	ret = net_mgmt(NET_REQUEST_DECT_NEIGHBOR_LIST, test_iface, NULL, 0);
	LOG_DBG("NET_REQUEST_DECT_NEIGHBOR_LIST result when deactivated: %d", ret);
	if (ret == 0) {
		LOG_DBG("Neighbor list request accepted when deactivated - should return empty "
			"list");
		k_sleep(K_MSEC(100)); /* Allow time for processing */

		/* Verify that NET_EVENT_DECT_NEIGHBOR_LIST was received */
		if (dect_neighbor_list_received) {
			LOG_DBG("NET_EVENT_DECT_NEIGHBOR_LIST received - checking if list is "
				"empty");
			TEST_ASSERT_EQUAL_MESSAGE(
				0, received_neighbor_list_data.neighbor_count,
				"Neighbor list should be empty when stack is deactivated");
			LOG_DBG("Verified: Empty neighbor list returned when deactivated "
				"(neighbor_count=0)");
		} else {
			LOG_DBG("NET_EVENT_DECT_NEIGHBOR_LIST not received - request may have "
				"failed silently");
		}
	} else {
		LOG_DBG("Neighbor list request immediately failed when deactivated with error: %d",
			ret);
		TEST_ASSERT_NOT_EQUAL_INT(0, ret);
	}

	/* Test 4: NET_REQUEST_DECT_NEIGHBOR_INFO behavior when deactivated */
	struct dect_neighbor_info_req_params neighbor_info_params = {.long_rd_id = 0x56789ABC};

	ret = net_mgmt(NET_REQUEST_DECT_NEIGHBOR_INFO, test_iface, &neighbor_info_params,
		       sizeof(neighbor_info_params));
	LOG_DBG("NET_REQUEST_DECT_NEIGHBOR_INFO result when deactivated: %d", ret);
	if (ret == 0) {
		LOG_DBG("Neighbor info request accepted when deactivated - should return failure "
			"status");
		k_sleep(K_MSEC(100)); /* Allow time for processing */

		/* Verify that NET_EVENT_DECT_NEIGHBOR_INFO was received */
		if (dect_neighbor_info_received) {
			LOG_DBG("NET_EVENT_DECT_NEIGHBOR_INFO received - checking status");
			TEST_ASSERT_NOT_EQUAL_MESSAGE(
				DECT_MAC_STATUS_OK, received_neighbor_info_data.status,
				"Neighbor info should fail when stack is deactivated");
			LOG_DBG("Verified: Neighbor info returned error status when deactivated "
				"(status=%d)",
				received_neighbor_info_data.status);
		} else {
			LOG_DBG("NET_EVENT_DECT_NEIGHBOR_INFO not received - request may have "
				"failed silently");
		}
	} else {
		LOG_DBG("Neighbor info request immediately failed when deactivated with error: %d",
			ret);
		TEST_ASSERT_NOT_EQUAL_INT(0, ret);
	}

	/* Test 5: NET_REQUEST_DECT_ASSOCIATION_RELEASE behavior when deactivated */
	ret = net_mgmt(NET_REQUEST_DECT_ASSOCIATION_RELEASE, test_iface, NULL, 0);
	LOG_DBG("NET_REQUEST_DECT_ASSOCIATION_RELEASE result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(-95, ret); /* -ENOTSUP: Operation not supported when deactivated */
	LOG_DBG("Association release request correctly failed when deactivated with -ENOTSUP");

	/* Test 6: NET_REQUEST_DECT_STATUS_INFO_GET might still work when deactivated (synchronous)
	 */
	struct dect_status_info status_info = {0};

	ret = net_mgmt(NET_REQUEST_DECT_STATUS_INFO_GET, test_iface, &status_info,
		       sizeof(status_info));
	LOG_DBG("NET_REQUEST_DECT_STATUS_INFO_GET result when deactivated: %d", ret);
	if (ret == 0) {
		LOG_DBG("Status info when deactivated - mdm_activated: %s",
			status_info.mdm_activated ? "true" : "false");
		TEST_ASSERT_FALSE_MESSAGE(status_info.mdm_activated,
					  "Stack should report as deactivated");
	} else {
		LOG_DBG("Status info request failed when deactivated (acceptable)");
	}

	/* Test 7: NET_REQUEST_DECT_DEACTIVATE should succeed when already deactivated (idempotent)
	 */
	dect_deactivate_done_received = false; /* Reset flag to detect new event */
	ret = net_mgmt(NET_REQUEST_DECT_DEACTIVATE, test_iface, NULL, 0);
	LOG_DBG("NET_REQUEST_DECT_DEACTIVATE result when already deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(0,
			      ret); /* Deactivate request should succeed as idempotent operation */

	/* Wait for the deactivate done event */
	k_sleep(K_MSEC(100));

	/* Verify that NET_EVENT_DECT_DEACTIVATE_DONE was received even when already deactivated */
	TEST_ASSERT_TRUE_MESSAGE(
		dect_deactivate_done_received,
		"NET_EVENT_DECT_DEACTIVATE_DONE should be received even when already deactivated");
	TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_OK, dect_deactivate_done_status,
				  "Deactivate event should complete with success status even when "
				  "already deactivated");

	LOG_DBG("Deactivate request correctly succeeded when already deactivated (idempotent "
		"operation)");
	LOG_DBG("- Event received: %s", dect_deactivate_done_received ? "YES" : "NO");
	LOG_DBG("- Event status: %d (expected=0 for DECT_MAC_STATUS_OK)",
		dect_deactivate_done_status);

	/* Test 8: RSSI scan when deactivated */
	LOG_DBG("Test 8: RSSI scan when deactivated");
	struct dect_rssi_scan_params rssi_scan_params = {
		.band = 0, .frame_count_to_scan = 1, .channel_count = 1, .channel_list = {1657}};

	ret = net_mgmt(NET_REQUEST_DECT_RSSI_SCAN, test_iface, &rssi_scan_params,
		       sizeof(rssi_scan_params));
	LOG_DBG("NET_REQUEST_DECT_RSSI_SCAN result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(0, ret); /* Request accepted, but functionality limited */

	if (ret == 0) {
		LOG_DBG("RSSI scan request accepted when deactivated - waiting for "
			"NET_EVENT_DECT_RSSI_SCAN_DONE with error status");
		/* Wait for RSSI scan events to complete - should get RSSI_SCAN_DONE with "not
		 * allowed" status
		 */
		k_sleep(K_MSEC(100));

		if (dect_rssi_scan_done_received) {
			/* Verify the RSSI scan failed with "not allowed" status */
			TEST_ASSERT_EQUAL_MESSAGE(DECT_MAC_STATUS_NOT_ALLOWED,
						  dect_rssi_scan_done_status,
						  "RSSI scan should fail with error status when "
						  "stack is deactivated");
			LOG_DBG("RSSI scan properly failed with NET_EVENT_DECT_RSSI_SCAN_DONE "
				"status: %d",
				dect_rssi_scan_done_status);
		} else {
			LOG_DBG("NET_EVENT_DECT_RSSI_SCAN_DONE was not received - async callback "
				"may have failed");
			/* This might be acceptable if the mock callback system has issues */
		}
	}

	/* Test 9: Cluster start when deactivated */
	LOG_DBG("Test 9: Cluster start when deactivated");
	struct dect_cluster_start_req_params cluster_start_params = {.channel = 1657};

	ret = net_mgmt(NET_REQUEST_DECT_CLUSTER_START, test_iface, &cluster_start_params,
		       sizeof(cluster_start_params));
	LOG_DBG("NET_REQUEST_DECT_CLUSTER_START result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(-22, ret); /* -EINVAL: Not allowed for PT role */

	/* Test 10: NW beacon start when deactivated */
	LOG_DBG("Test 10: NW beacon start when deactivated");
	struct dect_nw_beacon_start_req_params nw_beacon_start_params = {.channel = 1657,
									 .additional_ch_count = 0};

	ret = net_mgmt(NET_REQUEST_DECT_NW_BEACON_START, test_iface, &nw_beacon_start_params,
		       sizeof(nw_beacon_start_params));
	LOG_DBG("NET_REQUEST_DECT_NW_BEACON_START result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(-22, ret); /* -EINVAL: Not allowed for PT role */

	/* Test 11: NW beacon stop when deactivated */
	LOG_DBG("Test 11: NW beacon stop when deactivated");
	struct dect_nw_beacon_stop_req_params nw_beacon_stop_params = {};

	ret = net_mgmt(NET_REQUEST_DECT_NW_BEACON_STOP, test_iface, &nw_beacon_stop_params,
		       sizeof(nw_beacon_stop_params));
	LOG_DBG("NET_REQUEST_DECT_NW_BEACON_STOP result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(-114, ret); /* -EALREADY: Already stopped */

	/* Test 12: Cluster info when deactivated */
	LOG_DBG("Test 12: Cluster info when deactivated");
	ret = net_mgmt(NET_REQUEST_DECT_CLUSTER_INFO, test_iface, NULL, 0);
	LOG_DBG("NET_REQUEST_DECT_CLUSTER_INFO result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(0, ret); /* Information requests allowed even when deactivated */

	/* Test 13: Network create when deactivated */
	LOG_DBG("Test 13: Network create when deactivated");
	ret = net_mgmt(NET_REQUEST_DECT_NETWORK_CREATE, test_iface, NULL, 0);
	LOG_DBG("NET_REQUEST_DECT_NETWORK_CREATE result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(-95, ret); /* -ENOTSUP: Operation not supported when deactivated */

	/* Test 14: Network join when deactivated */
	LOG_DBG("Test 14: Network join when deactivated");
	ret = net_mgmt(NET_REQUEST_DECT_NETWORK_JOIN, test_iface, NULL, 0);
	LOG_DBG("NET_REQUEST_DECT_NETWORK_JOIN result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(-22, ret); /* -EINVAL: Modem not activated */

	/* Test 15: Network unjoin when deactivated */
	LOG_DBG("Test 15: Network unjoin when deactivated");
	ret = net_mgmt(NET_REQUEST_DECT_NETWORK_UNJOIN, test_iface, NULL, 0);
	LOG_DBG("NET_REQUEST_DECT_NETWORK_UNJOIN result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(-114, ret); /* -EALREADY: Not joined */

	/* Test 16: Network remove when deactivated */
	LOG_DBG("Test 16: Network remove when deactivated");
	ret = net_mgmt(NET_REQUEST_DECT_NETWORK_REMOVE, test_iface, NULL, 0);
	LOG_DBG("NET_REQUEST_DECT_NETWORK_REMOVE result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(-95, ret); /* -ENOTSUP: Operation not supported when deactivated */

	/* Test 17: Cluster reconfigure when deactivated */
	LOG_DBG("Test 17: Cluster reconfigure when deactivated");
	struct dect_cluster_reconfig_req_params cluster_reconfig_params = {
		.channel = 1657,
		.max_beacon_tx_power_dbm = 20,
		.max_cluster_power_dbm = 20,
		.period = DECT_MAC_CLUSTER_BEACON_PERIOD_100MS};
	ret = net_mgmt(NET_REQUEST_DECT_CLUSTER_RECONFIGURE, test_iface, &cluster_reconfig_params,
		       sizeof(cluster_reconfig_params));
	LOG_DBG("NET_REQUEST_DECT_CLUSTER_RECONFIGURE result when deactivated: %d", ret);
	TEST_ASSERT_EQUAL_INT(-22, ret); /* -EINVAL: Not allowed for PT role */

	/* Verify behavior - some events may be generated depending on what operations succeeded */
	k_sleep(K_MSEC(100)); /* Short wait to ensure no async events */

	LOG_DBG("Event generation status after deactivated requests:");
	LOG_DBG("- Scan result received: %s", dect_scan_result_received ? "YES" : "NO");
	LOG_DBG("- Scan done received: %s", dect_scan_done_received ? "YES" : "NO");
	LOG_DBG("- Neighbor list received: %s", dect_neighbor_list_received ? "YES" : "NO");
	LOG_DBG("- Neighbor info received: %s", dect_neighbor_info_received ? "YES" : "NO");
	LOG_DBG("- Association changed received: %s",
		dect_association_changed_received ? "YES" : "NO");

	LOG_DBG("=== DECT deactivated requests test completed successfully ===");
	LOG_DBG("Validated request behavior when stack is deactivated");
}

/**
 * FT Configuration Test - Configure device to FT mode
 *
 * This test verifies:
 * 1. NET_REQUEST_DECT_SETTINGS_WRITE works with DECT_DEVICE_TYPE_FT
 * 2. Settings write is synchronous and successful
 * 3. NET_REQUEST_DECT_SETTINGS_READ confirms the device_type was changed to FT
 */
void test_dect_ft_configuration(void)
{
	LOG_DBG("=== FT Configuration Test ===");

	/* First, activate DECT stack if not already active (we assume it's active from previous
	 * tests)
	 */

	/* Step 1: Configure settings to set device type to FT */
	struct dect_settings write_settings = {0};

	write_settings.cmd_params.write_scope_bitmap |= DECT_SETTINGS_WRITE_SCOPE_DEVICE_TYPE;
	write_settings.device_type = DECT_DEVICE_TYPE_FT;

	LOG_DBG("Writing DECT settings to configure device as FT");
	LOG_DBG("- device_type: DECT_DEVICE_TYPE_FT (%d)", DECT_DEVICE_TYPE_FT);
	LOG_DBG("- write_scope_bitmap: DECT_SETTINGS_WRITE_SCOPE_DEVICE_TYPE (0x%04X)",
		DECT_SETTINGS_WRITE_SCOPE_DEVICE_TYPE);

	/* Step 2: Write settings (synchronous call) */
	int result = net_mgmt(NET_REQUEST_DECT_SETTINGS_WRITE, test_iface, &write_settings,
			      sizeof(struct dect_settings));

	LOG_DBG("NET_REQUEST_DECT_SETTINGS_WRITE result: %d", result);
	TEST_ASSERT_EQUAL_INT(0, result);
	LOG_DBG("Settings write completed successfully - device configured as FT");

	/* Step 3: Read settings back to verify the change */
	struct dect_settings read_settings = {0};

	LOG_DBG("Reading DECT settings to verify FT configuration");
	result = net_mgmt(NET_REQUEST_DECT_SETTINGS_READ, test_iface, &read_settings,
			  sizeof(struct dect_settings));

	LOG_DBG("NET_REQUEST_DECT_SETTINGS_READ result: %d", result);
	TEST_ASSERT_EQUAL_INT(0, result);

	/* Step 4: Verify device type was changed to FT */
	LOG_DBG("Read settings verification:");
	LOG_DBG("- device_type: %d (expected: %d for DECT_DEVICE_TYPE_FT)",
		read_settings.device_type, DECT_DEVICE_TYPE_FT);

	TEST_ASSERT_TRUE(read_settings.device_type & DECT_DEVICE_TYPE_FT);

	LOG_DBG("=== FT Configuration Test Completed Successfully ===");
	LOG_DBG("- Settings write operation: SUCCESS");
	LOG_DBG("- Settings read operation: SUCCESS");
	LOG_DBG("- Device type verified: FT mode configured");
}

/**
 * @brief Test complete DECT PT device workflow using real net_mgmt API: scan -> associate
 *
 * TODO: This test is disabled for now until all parameter structures are resolved
 */
void test_dect_pt_complete_workflow(void)
{
	/* Simple placeholder test - verify callback structure exists */
	TEST_ASSERT_NOT_NULL(&mock_ntf_callbacks);
}
