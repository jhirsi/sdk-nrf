/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOSH_LOCATION_METRICS_H
#define MOSH_LOCATION_METRICS_H

int location_metrics_utils_json_payload_encode(const struct location_event_data *loc_evt_data,
					       int64_t timestamp_to_json, char **json_str_out);

#endif /* MOSH_LOCATION_METRICS_H */
