/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_NRF91_RX_H
#define DECT_NRF91_RX_H

#include <zephyr/kernel.h>

#define DECT_NRF91_RX_OP_RX_DATA_WITH_PKT_PTR 1

int dect_nrf91_rx_msgq_data_op_add(uint16_t event_id, void *data, size_t data_size);

#endif /* DECT_NRF91_RX_H */
