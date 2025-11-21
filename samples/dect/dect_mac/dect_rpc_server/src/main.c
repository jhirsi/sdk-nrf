/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <nrf_rpc.h>
#include <modem/nrf_modem_lib.h>
#include <nrf_modem.h>

#include <net/l2_dect_nrp/rpc/server/dect_rpc_server.h>
#include <net/l2_dect_nrp/rpc/common/dect_rpc_group.h>

#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
#include <net/nrf_cloud.h>
#include <net/l2_dect_nrp/rpc/common/dect_rpc_mqtt_transport.h>
#endif

LOG_MODULE_REGISTER(dect_rpc_server_sample, LOG_LEVEL_DBG);

static void err_handler(const struct nrf_rpc_err_report *report)
{
	LOG_ERR("nRF RPC error %d occurred. See nRF RPC logs for more details", report->code);
}

void nrf_modem_fault_handler(struct nrf_modem_fault_info *fault_info)
{
	LOG_ERR("Modem crash reason: 0x%x, PC: 0x%x", fault_info->reason,
		fault_info->program_counter);
	__ASSERT(false, "Modem crash detected, halting application execution");
}

#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
static void nrf_cloud_event_handler(const struct nrf_cloud_evt *evt)
{
	switch (evt->type) {
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTED:
		LOG_INF("nRF Cloud MQTT connected");
		break;
	case NRF_CLOUD_EVT_READY:
		LOG_INF("nRF Cloud ready");
		break;
	case NRF_CLOUD_EVT_RX_DATA_GENERAL:
		/* Check if this is a DECT RPC packet */
		if (dect_rpc_mqtt_transport_handle_rx_data(&evt->data)) {
			LOG_DBG("DECT RPC packet received and handled");
		}
		/* If not handled, it's other cloud data */
		break;
	case NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED:
		LOG_INF("nRF Cloud disconnected");
		break;
	case NRF_CLOUD_EVT_TRANSPORT_CONNECT_ERROR:
		LOG_ERR("nRF Cloud connection error: %d", evt->status);
		break;
	case NRF_CLOUD_EVT_ERROR:
		LOG_ERR("nRF Cloud error: %d", evt->status);
		break;
	default:
		break;
	}
}
#endif

int main(void)
{
	int ret;

	LOG_INF("DECT RPC Server Sample");

	/* Initialize nRF Modem library (required for DECT MAC) */
	ret = nrf_modem_lib_init();
	if (ret != 0) {
		LOG_ERR("nRF Modem library init failed: %d", ret);
		return ret;
	}
	LOG_INF("nRF Modem library initialized");

#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
	LOG_INF("Initializing nRF Cloud MQTT");

	struct nrf_cloud_init_param init_param = {
		.event_handler = nrf_cloud_event_handler,
	};

	ret = nrf_cloud_init(&init_param);
	if (ret != 0) {
		LOG_ERR("nRF Cloud init failed: %d", ret);
		return ret;
	}

	LOG_INF("Connecting to nRF Cloud...");
	ret = nrf_cloud_connect();
	if (ret != 0 && ret != NRF_CLOUD_CONNECT_RES_ERR_ALREADY_CONNECTED) {
		LOG_ERR("nRF Cloud connect failed: %d", ret);
		return ret;
	}

	/* Wait a bit for connection to establish */
	k_sleep(K_SECONDS(2));
#endif

	LOG_INF("Initializing RPC server");

	ret = nrf_rpc_init(err_handler);
	if (ret != 0) {
		LOG_ERR("RPC init failed: %d", ret);
		return ret;
	}

	LOG_INF("Initializing DECT RPC server");

	ret = dect_rpc_server_init();
	if (ret != 0) {
		LOG_ERR("DECT RPC server init failed: %d", ret);
		return ret;
	}

	LOG_INF("DECT RPC server ready");
#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
	LOG_INF("Waiting for RPC commands from cloud via MQTT...");
#else
	LOG_INF("Waiting for RPC commands from client...");
#endif

	return 0;
}

