/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef INTEGRATIONS_H
#define INTEGRATIONS_H

#include <modem/lte_lc.h>
#include <modem/location.h>

#define IMEI_LEN 15

/** @brief Location based metrics */
struct location_metrics {
	struct location_event_data location_data;
	struct lte_lc_cells_info cell_data;
	char device_imei_str[IMEI_LEN + 1];
	int bat_voltage;
	float temperature;
	char *ncell_meas_notif_str;
};

/**
 * @brief Send metrics data by using configured cloud strorage.
 *
 * @param[in]     metrics_data Data.
 *
 * @retval 0 If successful.
 *          Otherwise, a (negative) error code is returned.
 */
int rest_integration_metrics_data_send(const struct location_metrics *metrics_data);

#endif /* INTEGRATIONS_H */
