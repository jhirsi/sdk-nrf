/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DESH_AT_CMD_MODE_SETT_H
#define DESH_AT_CMD_MODE_SETT_H

int at_cmd_mode_sett_init(void);
int at_cmd_mode_sett_autostart_enabled(bool enabled);
bool at_cmd_mode_sett_is_autostart_enabled(void);

#endif /* DESH_AT_CMD_MODE_SETT_H */
