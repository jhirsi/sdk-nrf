/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef METHOD_CELLULAR_H
#define METHOD_CELLULAR_H

int method_cellular_location_request(const struct loc_method_config *config);
int method_cellular_init(void);
int method_cellular_cancel(void);

#endif /* METHOD_CELLULAR_H */
