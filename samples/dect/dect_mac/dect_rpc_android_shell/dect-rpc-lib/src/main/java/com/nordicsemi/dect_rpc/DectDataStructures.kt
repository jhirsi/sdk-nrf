/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc

import java.net.Inet6Address
import java.net.InetAddress

/**
 * DECT settings region
 */
enum class DectSettingsRegion(val value: UInt) {
    EU(0u),
    US(1u),
    GLOBAL(2u)
}

/**
 * DECT device type (bitmap flags)
 */
enum class DectDeviceType(val value: UInt) {
    NONE(0u),
    FT(0x01u),  // Fixed Terminal
    PT(0x02u)   // Portable Terminal
}

/**
 * Cluster beacon period
 */
enum class DectClusterBeaconPeriod(val value: UInt) {
    // Values match C enum
}

/**
 * Network beacon period
 */
enum class DectNwBeaconPeriod(val value: UInt) {
    // Values match C enum
}

/**
 * Security mode
 */
enum class DectMacSecurityMode(val value: UInt) {
    NONE(0u),
    MODE1(1u)
}

/**
 * DECT association data
 */
data class DectAssociationData(
    var longRdId: UInt = 0u,
    var localIpv6Addr: Inet6Address? = null,
    var globalIpv6AddrSet: Boolean = false,
    var globalIpv6Addr: Inet6Address? = null
)

/**
 * DECT status information
 */
data class DectStatusInfo(
    var modemActivated: Boolean = false,
    var clusterRunning: Boolean = false,
    var clusterChannel: UShort = 0u,
    var nwBeaconRunning: Boolean = false,
    var parentCount: UByte = 0u,
    var parentAssociations: Array<DectAssociationData> = emptyArray(),
    var childCount: UByte = 0u,
    var childAssociations: Array<DectAssociationData> = emptyArray(),
    
    // Border router info
    var brNetIfaceIndex: Int = -1, // -1 if not set
    var brGlobalIpv6AddrPrefixSet: Boolean = false,
    var brGlobalIpv6AddrPrefix: Inet6Address? = null,
    var brGlobalIpv6AddrPrefixLen: Int = 0,
    
    // Firmware version
    var fwVersionStr: String = ""
)

/**
 * DECT settings
 */
data class DectSettings(
    // Command params
    var resetToDriverDefaults: Boolean = false,
    var writeScopeBitmap: UShort = 0u,

    // Auto-start
    var autoStartActivate: Boolean = false,

    // Region
    var region: DectSettingsRegion = DectSettingsRegion.EU,

    // Device type
    var deviceType: DectDeviceType = DectDeviceType.NONE,

    // Identities
    var networkId: UInt = 0u,
    var transmitterLongRdId: UInt = 0u,

    // TX settings
    var maxPowerDbm: Byte = 0,
    var maxMcs: UByte = 0u,

    // Power save
    var powerSave: Boolean = false,

    // Band
    var bandNbr: UByte = 0u,

    // RSSI scan
    var rssiScanSuitablePercent: UByte = 0u,
    var rssiScanTimePerChannelMs: UInt = 0u,
    var rssiScanFreeThresholdDbm: Int = 0,
    var rssiScanBusyThresholdDbm: Int = 0,

    // Cluster settings
    var clusterMaxBeaconTxPowerDbm: Byte = 0,
    var clusterMaxClusterPowerDbm: Byte = 0,
    var clusterBeaconPeriod: DectClusterBeaconPeriod? = null,
    var clusterMaxNumNeighbors: UShort = 0u,
    var clusterNeighborInactivityDisconnectTimerMs: UInt = 0u,
    var clusterChannelLoadedPercent: UByte = 0u,

    // Network beacon
    var nwBeaconChannel: UShort = 0u,
    var nwBeaconPeriod: DectNwBeaconPeriod? = null,

    // Association
    var associationMaxBeaconRxFailures: UByte = 0u,
    var associationMinSensitivityDbm: Byte = 0,

    // Network join
    var networkJoinTargetFtLongRdId: UInt = 0u,

    // Security
    var securityMode: DectMacSecurityMode = DectMacSecurityMode.NONE,
    var securityIntegrityKey: ByteArray = ByteArray(16),
    var securityCipherKey: ByteArray = ByteArray(16)
)

/**
 * DECT RPC event
 */
data class DectRpcEvent(
    var ifaceIndex: Int = 0,
    var eventId: DectRpcEvtClient = DectRpcEvtClient.ACTIVATE_DONE,
    var eventData: ByteArray? = null
)


