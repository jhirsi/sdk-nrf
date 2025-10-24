/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>

#include <nrf_modem_dect_mac.h>

#include <dect_net_l2_mgmt.h>
#include <net/dect_nrp_utils.h>


#include "dect_nrf91_common.h"
#include "dect_nrf91_ctrl.h"
#include "dect_nrf91_settings.h"

#include "dect_nrf91_rx.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(DECT_NRP_MAC, CONFIG_DECT_NRP_MAC_LOG_LEVEL);

/**************************************************************************************************/

static bool dect_nrf91_data_rx_with_pkt_ptr(struct dect_nrf91_ctrl_dlc_rx_data_with_pkt_ptr *params)
{
	struct net_linkaddr ll_src;
	struct net_linkaddr ll_dst;
	/* Pkt has been allocated and written, now set the addressing part */
	struct net_pkt *rcv_pkt = params->pkt;

	int ret;
	bool handled = false;

	__ASSERT_NO_MSG(rcv_pkt != NULL);

	/* Set ll source and destination addresses based on long RD IDs:
	 * src as received and dst as configured in this device for long rd id
	 */
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	dect_nrp_utils_net_linkaddr_set_from_long_rd_id(&ll_src, params->mdm_params.long_rd_id);
	dect_nrp_utils_net_linkaddr_set_from_long_rd_id(
		&ll_dst, set_ptr->net_mgmt_common.identities.transmitter_long_rd_id);

	ret = net_linkaddr_set(net_pkt_lladdr_dst(rcv_pkt), ll_dst.addr, ll_dst.len);
	if (ret < 0) {
		LOG_ERR("%s: cannot set destination link address, ret %d", (__func__), ret);
		net_pkt_unref(rcv_pkt);
		return false;
	}

	ret = net_linkaddr_set(net_pkt_lladdr_src(rcv_pkt), ll_src.addr, ll_src.len);
	if (ret < 0) {
		LOG_ERR("%s: cannot set source link address, ret %d", (__func__), ret);
		net_pkt_unref(rcv_pkt);
		return false;
	}

	ret = net_recv_data(params->iface, rcv_pkt);
	if (ret < 0) {
		LOG_ERR("%s: received packet dropped from %u (%d bytes), ret %d", (__func__),
			params->mdm_params.long_rd_id, params->data_len, ret);
		net_pkt_unref(rcv_pkt);
	} else {
		LOG_DBG("%s: received packet from %u (%d bytes)", (__func__),
			params->mdm_params.long_rd_id, params->data_len);
		handled = true;
	}
	return handled;
}

/**************************************************************************************************/

K_MSGQ_DEFINE(dect_nrf91_rx_th_op_event_msgq, sizeof(struct dect_nrf91_common_op_event_msgq_item),
	      10, 4);

/**************************************************************************************************/

static void dect_nrf91_rx_th_op_handler_thread_fn(void)
{
	struct dect_nrf91_common_op_event_msgq_item event;

	while (true) {
		k_msgq_get(&dect_nrf91_rx_th_op_event_msgq, &event, K_FOREVER);

		switch (event.id) {
		case DECT_NRF91_RX_OP_RX_DATA_WITH_PKT_PTR: {
			struct dect_nrf91_ctrl_dlc_rx_data_with_pkt_ptr *params =
				(struct dect_nrf91_ctrl_dlc_rx_data_with_pkt_ptr *)event.data;
			bool data_handled = false;

			LOG_DBG("DLC data received to iface %p, transmitter: %u (0x%X), "
				"flow ID: %hhu, data_len: %u",
				params->iface, params->mdm_params.long_rd_id,
				params->mdm_params.long_rd_id, params->mdm_params.flow_id,
				params->data_len);

			data_handled = dect_nrf91_data_rx_with_pkt_ptr(params);
			if (!data_handled) {
				LOG_ERR("DECT_NRF91_RX_OP_RX_DATA_WITH_PKT_PTR: Cannot pass DLC RX "
					"data upwards in stack (len %d)",
					params->data_len);
			}
			break;
		}
		default:
			LOG_ERR("DECT RX: Unknown event %d received", event.id);
			break;
		}
		k_free(event.data);
	}
}
#define DECT_PHY_RX_THREAD_STACK_SIZE CONFIG_DECT_NRP_MAC_NRF_RX_THREAD_STACK_SIZE
#define DECT_PHY_RX_THREAD_PRIORITY   5

K_THREAD_DEFINE(dect_nrf91_rx_th, DECT_PHY_RX_THREAD_STACK_SIZE,
		dect_nrf91_rx_th_op_handler_thread_fn, NULL, NULL, NULL,
		K_PRIO_PREEMPT(DECT_PHY_RX_THREAD_PRIORITY), 0, 0);

/**************************************************************************************************/

int dect_nrf91_rx_msgq_data_op_add(uint16_t event_id, void *data, size_t data_size)
{
	int ret = 0;
	struct dect_nrf91_common_op_event_msgq_item event;

	event.data = k_malloc(data_size);
	if (event.data == NULL) {
		return -ENOMEM;
	}
	memcpy(event.data, data, data_size);

	event.id = event_id;
	ret = k_msgq_put(&dect_nrf91_rx_th_op_event_msgq, &event, K_NO_WAIT);
	if (ret) {
		k_free(event.data);
		return -ENOBUFS;
	}
	return 0;
}
