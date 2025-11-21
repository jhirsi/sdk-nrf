/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc_shell.models

import java.util.Date

/**
 * Device information from nRF Cloud
 */
data class CloudDevice(
    val id: String,
    val name: String,
    val type: String? = null,
    val online: Boolean = false,
    val lastSeen: Date? = null,
    val protocol: String? = null,  // MQTT, CoAP, etc.
    val connectionStatus: String? = null,  // connected, disconnected, etc.
    val method: String? = null,  // Connection method
    val firmware: FirmwareInfo? = null,
    val batteryVoltage: Double? = null,
    val board: String? = null,  // Board name (e.g., nrf9151dk)
    val hardwareVersion: String? = null,  // Hardware version
    val tenantId: String? = null  // Tenant ID from nRF Cloud
) {
    data class FirmwareInfo(
        val appName: String? = null,
        val appVersion: String? = null,
        val modemVersion: String? = null
    )
}


