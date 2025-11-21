/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dect_rpc_mqtt_transport.h"
#include <nrf_rpc_tr.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <net/nrf_cloud.h>
#include <zephyr/sys/base64.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(dect_rpc_mqtt, CONFIG_DECT_NRP_RPC_LOG_LEVEL);

/* nRF RPC header size */
#define NRF_RPC_HEADER_SIZE 5

/* Minimum packet size (header + some data) */
#define DECT_RPC_MIN_PACKET_SIZE (NRF_RPC_HEADER_SIZE + 1)

/* Magic marker to identify DECT RPC packets */
/* nRF RPC header format: [version(1)][src_group_id(1)][dst_group_id(1)][id(1)][flags(1)] */
/* JSON data from cloud typically starts with '{' (0x7B) */
#define DECT_RPC_JSON_MAGIC 0x7B  /* '{' - if data starts with this, it's JSON, not RPC */

struct dect_rpc_mqtt_ctx {
	nrf_rpc_tr_receive_handler_t receive_cb;
	void *receive_ctx;
	bool initialized;
};

static struct dect_rpc_mqtt_ctx mqtt_ctx = {
	.initialized = false,
};

K_MUTEX_DEFINE(mqtt_ctx_lock);

/* Forward declaration */
const struct nrf_rpc_tr dect_rpc_mqtt_transport;

/**
 * @brief Check if data looks like an nRF RPC packet
 *
 * Simple heuristic: RPC packets are binary and have at least header size.
 * JSON data from cloud typically starts with '{'.
 */
static bool is_rpc_packet(const uint8_t *data, size_t len)
{
	/* Validate input parameters */
	if (!data || len == 0) {
		return false;
	}

	/* Must have at least RPC header size */
	if (len < NRF_RPC_HEADER_SIZE) {
		return false;
	}

	/* If it starts with '{', it's likely JSON, not RPC */
	if (data[0] == DECT_RPC_JSON_MAGIC) {
		return false;
	}

	/* Additional check: RPC packets are typically binary, not text */
	/* Check if first few bytes are printable ASCII (unlikely for RPC) */
	/* Limit check to avoid accessing beyond buffer */
	size_t check_len = (len < 10) ? len : 10;
	bool looks_like_text = true;
	for (size_t i = 0; i < check_len; i++) {
		if (data[i] < 0x20 || data[i] > 0x7E) {
			looks_like_text = false;
			break;
		}
	}

	/* If it looks like text and starts with '{', it's JSON */
	if (looks_like_text && data[0] == '{') {
		return false;
	}

	/* Otherwise, assume it could be an RPC packet */
	return true;
}

/**
 * @brief Handle nRF Cloud RX_DATA_GENERAL event
 */
bool dect_rpc_mqtt_transport_handle_rx_data(const struct nrf_cloud_data *data)
{
	if (!data || !data->ptr || data->len == 0) {
		return false;
	}

	/* Validate data length is reasonable */
	if (data->len > 4096) {
		LOG_ERR("Data length too large: %zu", data->len);
		return false;
	}

	k_mutex_lock(&mqtt_ctx_lock, K_FOREVER);

	if (!mqtt_ctx.initialized || !mqtt_ctx.receive_cb) {
		k_mutex_unlock(&mqtt_ctx_lock);
		return false;
	}

	/* Check if this looks like an nRF RPC packet */
	/* Do this check before allocating buffer */
	if (!is_rpc_packet(data->ptr, data->len)) {
		k_mutex_unlock(&mqtt_ctx_lock);
		return false;
	}

	LOG_DBG("Received DECT RPC packet, len: %zu", data->len);

	/* Allocate buffer for payload (nRF RPC will free it via rx_buf_free) */
	uint8_t *payload_buf = k_malloc(data->len);
	if (!payload_buf) {
		LOG_ERR("Failed to allocate payload buffer of size %zu", data->len);
		k_mutex_unlock(&mqtt_ctx_lock);
		return true; /* We handled it, but failed */
	}

	/* Copy data safely */
	memcpy(payload_buf, data->ptr, data->len);

	/* Call nRF RPC receive handler */
	/* This should be safe now as we're in work queue context */
	mqtt_ctx.receive_cb(&dect_rpc_mqtt_transport, payload_buf,
			    data->len, mqtt_ctx.receive_ctx);

	k_mutex_unlock(&mqtt_ctx_lock);
	return true; /* Event was handled */
}

/**
 * @brief Initialize MQTT transport
 */
static int mqtt_transport_init(const struct nrf_rpc_tr *transport,
			       nrf_rpc_tr_receive_handler_t receive_cb,
			       void *context)
{
	k_mutex_lock(&mqtt_ctx_lock, K_FOREVER);

	if (mqtt_ctx.initialized) {
		LOG_WRN("MQTT transport already initialized");
		k_mutex_unlock(&mqtt_ctx_lock);
		return 0;
	}

	mqtt_ctx.receive_cb = receive_cb;
	mqtt_ctx.receive_ctx = context;
	mqtt_ctx.initialized = true;

	LOG_INF("DECT RPC MQTT transport initialized (using nRF Cloud events)");

	k_mutex_unlock(&mqtt_ctx_lock);
	return 0;
}

/**
 * @brief Send packet over MQTT using nRF Cloud
 * Wraps the RPC response in JSON format: {"appId":"DECT_RPC","data":"<base64>"}
 */
static int mqtt_transport_send(const struct nrf_rpc_tr *transport,
				const uint8_t *data, size_t length)
{
	int ret;
	size_t base64_len = 0;
	size_t json_len = 0;
	uint8_t *base64_buf = NULL;
	char *json_buf = NULL;

	k_mutex_lock(&mqtt_ctx_lock, K_FOREVER);

	if (!mqtt_ctx.initialized) {
		LOG_ERR("MQTT transport not initialized");
		k_mutex_unlock(&mqtt_ctx_lock);
		return -ENOTCONN;
	}

	k_mutex_unlock(&mqtt_ctx_lock);

	LOG_DBG("Sending DECT RPC packet via nRF Cloud, len: %zu", length);

	/* Calculate base64 encoded length: (length * 4 / 3) + padding + null terminator */
	base64_len = ((length * 4) / 3) + 4 + 1; /* +4 for padding, +1 for null terminator */
	base64_buf = k_malloc(base64_len);
	if (!base64_buf) {
		LOG_ERR("Failed to allocate base64 buffer");
		return -ENOMEM;
	}

	/* Encode binary data to base64 */
	ret = base64_encode(base64_buf, base64_len, &base64_len, data, length);
	if (ret < 0) {
		LOG_ERR("Failed to base64 encode RPC packet: %d", ret);
		k_free(base64_buf);
		return ret;
	}

	/* Ensure null termination */
	base64_buf[base64_len] = '\0';

	/* Calculate JSON buffer size: {"appId":"DECT_RPC","data":"<base64>"} */
	/* Format: {"appId":"DECT_RPC","data":"<base64_string>"} */
	json_len = strlen("{\"appId\":\"DECT_RPC\",\"data\":\"\"}") + base64_len + 1;
	json_buf = k_malloc(json_len);
	if (!json_buf) {
		LOG_ERR("Failed to allocate JSON buffer");
		k_free(base64_buf);
		return -ENOMEM;
	}

	/* Create JSON string */
	ret = snprintf(json_buf, json_len, "{\"appId\":\"DECT_RPC\",\"data\":\"%s\"}",
		       (char *)base64_buf);
	if (ret < 0 || ret >= (int)json_len) {
		LOG_ERR("Failed to create JSON string");
		k_free(base64_buf);
		k_free(json_buf);
		return -EINVAL;
	}

	LOG_DBG("Sending JSON-wrapped RPC response: %s", json_buf);

	/* Use nRF Cloud send API with MESSAGE topic.
	 * Send JSON-wrapped data with appId "DECT_RPC" and base64-encoded RPC packet.
	 * The nRF Cloud server will add its own receivedAt timestamp when it receives the message.
	 */
	struct nrf_cloud_tx_data tx_data = {
		.topic_type = NRF_CLOUD_TOPIC_MESSAGE,
		.data = {
			.ptr = (void *)json_buf,
			.len = strlen(json_buf)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE
	};

	ret = nrf_cloud_send(&tx_data);
	if (ret < 0) {
		LOG_ERR("Failed to send via nRF Cloud: %d", ret);
	}

	/* Free allocated buffers */
	k_free(base64_buf);
	k_free(json_buf);

	return ret;
}

/**
 * @brief Allocate TX buffer
 */
static void *mqtt_tx_buf_alloc(const struct nrf_rpc_tr *transport, size_t *size)
{
	void *buf = k_malloc(*size);
	if (!buf) {
		LOG_ERR("Failed to allocate TX buffer of size %zu", *size);
		*size = 0;
		return NULL;
	}
	return buf;
}

/**
 * @brief Free TX buffer
 */
static void mqtt_tx_buf_free(const struct nrf_rpc_tr *transport, void *buf)
{
	k_free(buf);
}

/**
 * @brief Free RX buffer
 */
static void mqtt_rx_buf_free(const struct nrf_rpc_tr *transport, void *buf)
{
	k_free(buf);
}

/* Transport API structure */
static const struct nrf_rpc_tr_api mqtt_transport_api = {
	.init = mqtt_transport_init,
	.send = mqtt_transport_send,
	.tx_buf_alloc = mqtt_tx_buf_alloc,
	.tx_buf_free = mqtt_tx_buf_free,
	.rx_buf_free = mqtt_rx_buf_free,
};

/* Transport instance - not static so it can be accessed from dect_rpc_group.c */
const struct nrf_rpc_tr dect_rpc_mqtt_transport = {
	.api = &mqtt_transport_api,
	.ctx = &mqtt_ctx,
};

/* Public API implementation */
const struct nrf_rpc_tr *dect_rpc_mqtt_transport_get(void)
{
	return &dect_rpc_mqtt_transport;
}
