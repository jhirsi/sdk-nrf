/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef METHOD_CELLULAR_H
#define METHOD_CELLULAR_H

int method_cellular_location_get(const struct location_method_config *config);
int method_cellular_init(void);
int method_cellular_cancel(void);

#if defined(CONFIG_LOCATION_METRICS)
bool method_cellular_metrics_get(struct location_event_data_metrics *metrics);
#endif

#endif /* METHOD_CELLULAR_H */
