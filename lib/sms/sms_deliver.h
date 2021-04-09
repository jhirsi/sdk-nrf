/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _SMS_DELIVER_INCLUDE_H_
#define _SMS_DELIVER_INCLUDE_H_

/* Forward declaration */
struct sms_data;

int sms_deliver_pdu_parse(char *pdu, struct sms_data *out);

#endif

