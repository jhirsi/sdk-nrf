/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc

import co.nstant.`in`.cbor.CborDecoder
import co.nstant.`in`.cbor.CborEncoder
import co.nstant.`in`.cbor.model.Array
import co.nstant.`in`.cbor.model.ByteString
import co.nstant.`in`.cbor.model.DataItem
import co.nstant.`in`.cbor.model.NegativeInteger
import co.nstant.`in`.cbor.model.SimpleValue
import co.nstant.`in`.cbor.model.SimpleValueType
import co.nstant.`in`.cbor.model.UnicodeString
import co.nstant.`in`.cbor.model.UnsignedInteger
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.net.Inet6Address
import java.net.InetAddress

/**
 * CBOR serialization helpers for DECT structures
 */
internal object CborSerializer {
    /**
     * Encode interface index
     * Returns just the integer value (not wrapped in array)
     * The nRF RPC decoder is initialized with ZCBOR_ARRAY_SIZE, which means
     * it expects a sequence of CBOR values that it will automatically wrap in an array.
     * When the C client uses nrf_rpc_encode_int(), it encodes just the integer,
     * and the nRF RPC library handles the array wrapping.
     * So we should send just the integer value, not a pre-encoded array.
     */
    fun encodeIfaceIndex(ifaceIndex: Int): UnsignedInteger {
        return UnsignedInteger(ifaceIndex.toLong())
    }

    /**
     * Decode integer result
     * Response format: sequence of CBOR values [result_code]
     */
    fun decodeIntResult(cborData: ByteArray): Int {
        // Decode all CBOR items from the sequence
        val bais = ByteArrayInputStream(cborData)
        val decoder = CborDecoder(bais)
        val allItems = decoder.decode()
        
        if (allItems.isEmpty()) {
            throw IllegalStateException("Invalid response format: no CBOR items found")
        }
        
        // First (and only) item is the result code
        return getIntValue(allItems[0])
    }

    /**
     * Encode DECT settings
     */
    fun encodeSettings(settings: DectSettings): Array {
        val array = Array()
        
        // Command params
        array.add(if (settings.resetToDriverDefaults) SimpleValue(SimpleValueType.TRUE) else SimpleValue(SimpleValueType.FALSE))
        array.add(UnsignedInteger(settings.writeScopeBitmap.toLong()))
        
        // Auto-start
        array.add(if (settings.autoStartActivate) SimpleValue(SimpleValueType.TRUE) else SimpleValue(SimpleValueType.FALSE))
        
        // Region
        array.add(UnsignedInteger(settings.region.value.toLong()))
        
        // Device type
        array.add(UnsignedInteger(settings.deviceType.value.toLong()))
        
        // Identities
        array.add(UnsignedInteger(settings.networkId.toLong()))
        array.add(UnsignedInteger(settings.transmitterLongRdId.toLong()))
        
        // TX settings
        array.add(if (settings.maxPowerDbm < 0) NegativeInteger(-settings.maxPowerDbm.toLong()) else UnsignedInteger(settings.maxPowerDbm.toLong()))
        array.add(UnsignedInteger(settings.maxMcs.toLong()))
        
        // Power save
        array.add(if (settings.powerSave) SimpleValue(SimpleValueType.TRUE) else SimpleValue(SimpleValueType.FALSE))
        
        // Band
        array.add(UnsignedInteger(settings.bandNbr.toLong()))
        
        // RSSI scan
        array.add(UnsignedInteger(settings.rssiScanSuitablePercent.toLong()))
        array.add(UnsignedInteger(settings.rssiScanTimePerChannelMs.toLong()))
        array.add(if (settings.rssiScanFreeThresholdDbm < 0) NegativeInteger(-settings.rssiScanFreeThresholdDbm.toLong()) else UnsignedInteger(settings.rssiScanFreeThresholdDbm.toLong()))
        array.add(if (settings.rssiScanBusyThresholdDbm < 0) NegativeInteger(-settings.rssiScanBusyThresholdDbm.toLong()) else UnsignedInteger(settings.rssiScanBusyThresholdDbm.toLong()))
        
        // Cluster settings
        array.add(if (settings.clusterMaxBeaconTxPowerDbm < 0) NegativeInteger(-settings.clusterMaxBeaconTxPowerDbm.toLong()) else UnsignedInteger(settings.clusterMaxBeaconTxPowerDbm.toLong()))
        array.add(if (settings.clusterMaxClusterPowerDbm < 0) NegativeInteger(-settings.clusterMaxClusterPowerDbm.toLong()) else UnsignedInteger(settings.clusterMaxClusterPowerDbm.toLong()))
        array.add(UnsignedInteger((settings.clusterBeaconPeriod?.value ?: 0u).toLong()))
        array.add(UnsignedInteger(settings.clusterMaxNumNeighbors.toLong()))
        array.add(UnsignedInteger(settings.clusterNeighborInactivityDisconnectTimerMs.toLong()))
        array.add(UnsignedInteger(settings.clusterChannelLoadedPercent.toLong()))
        
        // Network beacon
        array.add(UnsignedInteger(settings.nwBeaconChannel.toLong()))
        array.add(UnsignedInteger((settings.nwBeaconPeriod?.value ?: 0u).toLong()))
        
        // Association
        array.add(UnsignedInteger(settings.associationMaxBeaconRxFailures.toLong()))
        array.add(if (settings.associationMinSensitivityDbm < 0) NegativeInteger(-settings.associationMinSensitivityDbm.toLong()) else UnsignedInteger(settings.associationMinSensitivityDbm.toLong()))
        
        // Network join
        array.add(UnsignedInteger(settings.networkJoinTargetFtLongRdId.toLong()))
        
        // Security
        array.add(UnsignedInteger(settings.securityMode.value.toLong()))
        array.add(ByteString(settings.securityIntegrityKey))
        array.add(ByteString(settings.securityCipherKey))
        
        return array
    }

    /**
     * Decode DECT settings from response (skips result code)
     * Response format: sequence of CBOR values [result_code, settings_sequence...]
     */
    fun decodeSettingsFromResponse(cborData: ByteArray): DectSettings {
        // Decode all CBOR items from the sequence (result code + settings sequence)
        val bais = ByteArrayInputStream(cborData)
        val decoder = CborDecoder(bais)
        val allItems = decoder.decode()
        
        if (allItems.isEmpty()) {
            throw IllegalStateException("Invalid response format: no CBOR items found")
        }
        
        if (allItems.size < 2) {
            throw IllegalStateException("Invalid response format: expected at least 2 items (result_code + settings), got ${allItems.size}")
        }

        // First element is result code, remaining items are settings
        val resultCode = getIntValue(allItems[0])
        if (resultCode != 0) {
            throw IllegalStateException("Settings read failed with error: $resultCode")
        }

        // Remaining items form the settings sequence
        val settingsItems = allItems.subList(1, allItems.size)
        
        // Encode settings items as CBOR array
        val settingsArray = Array()
        settingsItems.forEach { settingsArray.add(it) }
        val settingsBytes = encodeCbor(settingsArray)
        
        return decodeSettings(settingsBytes)
    }

    /**
     * Decode DECT settings
     */
    fun decodeSettings(cborData: ByteArray): DectSettings {
        val obj = decodeCbor(cborData)
        
        // Check for null
        if (obj is SimpleValue && obj.simpleValueType == SimpleValueType.NULL) {
            throw IllegalStateException("Settings is null")
        }
        
        val items = if (obj is Array) obj.getDataItems() else throw IllegalStateException("Invalid settings format")

        val settings = DectSettings()
        var idx = 0

        // Command params
        settings.resetToDriverDefaults = decodeBoolean(items[idx++])
        settings.writeScopeBitmap = getIntValue(items[idx++]).toUShort()
        
        // Auto-start
        settings.autoStartActivate = decodeBoolean(items[idx++])
        
        // Region
        settings.region = DectSettingsRegion.values().find { 
            it.value == getIntValue(items[idx++]).toUInt()
        } ?: DectSettingsRegion.EU
        
        // Device type
        settings.deviceType = DectDeviceType.values().find { 
            it.value == getIntValue(items[idx++]).toUInt()
        } ?: DectDeviceType.NONE
        
        // Identities
        settings.networkId = getIntValue(items[idx++]).toUInt()
        settings.transmitterLongRdId = getIntValue(items[idx++]).toUInt()
        
        // TX settings
        settings.maxPowerDbm = getIntValue(items[idx++]).toByte()
        settings.maxMcs = getIntValue(items[idx++]).toUByte()
        
        // Power save
        settings.powerSave = decodeBoolean(items[idx++])
        
        // Band
        settings.bandNbr = getIntValue(items[idx++]).toUByte()
        
        // RSSI scan
        settings.rssiScanSuitablePercent = getIntValue(items[idx++]).toUByte()
        settings.rssiScanTimePerChannelMs = getIntValue(items[idx++]).toUInt()
        settings.rssiScanFreeThresholdDbm = getIntValue(items[idx++])
        settings.rssiScanBusyThresholdDbm = getIntValue(items[idx++])
        
        // Cluster settings
        settings.clusterMaxBeaconTxPowerDbm = getIntValue(items[idx++]).toByte()
        settings.clusterMaxClusterPowerDbm = getIntValue(items[idx++]).toByte()
        settings.clusterBeaconPeriod = DectClusterBeaconPeriod.values().find { 
            it.value == getIntValue(items[idx++]).toUInt()
        }
        settings.clusterMaxNumNeighbors = getIntValue(items[idx++]).toUShort()
        settings.clusterNeighborInactivityDisconnectTimerMs = getIntValue(items[idx++]).toUInt()
        settings.clusterChannelLoadedPercent = getIntValue(items[idx++]).toUByte()
        
        // Network beacon
        settings.nwBeaconChannel = getIntValue(items[idx++]).toUShort()
        settings.nwBeaconPeriod = DectNwBeaconPeriod.values().find { 
            it.value == getIntValue(items[idx++]).toUInt()
        }
        
        // Association
        settings.associationMaxBeaconRxFailures = getIntValue(items[idx++]).toUByte()
        settings.associationMinSensitivityDbm = getIntValue(items[idx++]).toByte()
        
        // Network join
        settings.networkJoinTargetFtLongRdId = getIntValue(items[idx++]).toUInt()
        
        // Security
        settings.securityMode = DectMacSecurityMode.values().find { 
            it.value == getIntValue(items[idx++]).toUInt()
        } ?: DectMacSecurityMode.NONE
        settings.securityIntegrityKey = (items[idx++] as ByteString).bytes
        settings.securityCipherKey = (items[idx++] as ByteString).bytes

        return settings
    }

    /**
     * Decode DECT status info from response (skips result code)
     * Response format: sequence of CBOR values [result_code, status_info_sequence...]
     * The device sends a sequence, not a CBOR array, so we need to decode all items
     */
    fun decodeStatusInfoFromResponse(cborData: ByteArray): DectStatusInfo {
        android.util.Log.d("CborSerializer", "decodeStatusInfoFromResponse: payload len=${cborData.size}, hex=${cborData.take(20).joinToString(" ") { "%02X".format(it) }}${if (cborData.size > 20) "..." else ""}")
        
        // Decode all CBOR items from the sequence (result code + status_info sequence)
        val bais = ByteArrayInputStream(cborData)
        val decoder = CborDecoder(bais)
        val allItems = decoder.decode()
        
        if (allItems.isEmpty()) {
            throw IllegalStateException("Invalid response format: no CBOR items found")
        }
        
        android.util.Log.d("CborSerializer", "decodeStatusInfoFromResponse: decoded ${allItems.size} CBOR items")
        
        // First item is result code
        val resultCode = getIntValue(allItems[0])
        android.util.Log.d("CborSerializer", "decodeStatusInfoFromResponse: result code=$resultCode")
        
        if (resultCode != 0) {
            throw IllegalStateException("Status info get failed with error: $resultCode")
        }
        
        if (allItems.size < 2) {
            throw IllegalStateException("Invalid response format: expected at least 2 items (result_code + status_info), got ${allItems.size}")
        }
        
        // Remaining items form the status_info sequence
        // We need to encode them back to CBOR bytes to pass to decodeStatusInfo
        // Create a new array with the status_info items (skip first item which is result code)
        val statusInfoItems = allItems.subList(1, allItems.size)
        android.util.Log.d("CborSerializer", "decodeStatusInfoFromResponse: status_info has ${statusInfoItems.size} items")
        
        // Encode status_info items as CBOR array
        val statusArray = Array()
        statusInfoItems.forEach { statusArray.add(it) }
        val statusBytes = encodeCbor(statusArray)
        
        return decodeStatusInfo(statusBytes)
    }

    /**
     * Decode DECT status info
     * C encoding format: sequential values (not nested arrays for associations)
     */
    fun decodeStatusInfo(cborData: ByteArray): DectStatusInfo {
        val obj = decodeCbor(cborData)
        
        // Check for null
        if (obj is SimpleValue && obj.simpleValueType == SimpleValueType.NULL) {
            throw IllegalStateException("Status info is null")
        }
        
        val items = if (obj is Array) obj.getDataItems() else throw IllegalStateException("Invalid status info format")

        val status = DectStatusInfo()
        var idx = 0

        // Basic fields
        status.modemActivated = decodeBoolean(items[idx++])
        status.clusterRunning = decodeBoolean(items[idx++])
        status.clusterChannel = getIntValue(items[idx++]).toUShort()
        status.nwBeaconRunning = decodeBoolean(items[idx++])
        
        // Parent associations - decode sequentially
        var parentCount = getIntValue(items[idx++]).toUByte()
        if (parentCount > 1u) {
            parentCount = 1u // Cap at 1 (as per C code)
        }
        status.parentCount = parentCount
        
        status.parentAssociations = Array(parentCount.toInt()) {
            val assoc = DectAssociationData()
            assoc.longRdId = getIntValue(items[idx++]).toUInt()
            assoc.localIpv6Addr = decodeIpv6Addr(items[idx++])
            assoc.globalIpv6AddrSet = decodeBoolean(items[idx++])
            if (assoc.globalIpv6AddrSet) {
                assoc.globalIpv6Addr = decodeIpv6Addr(items[idx++])
            }
            assoc
        }
        
        // Child associations - decode sequentially
        val childCount = getIntValue(items[idx++]).toUByte()
        status.childCount = childCount
        status.childAssociations = Array(childCount.toInt()) {
            val assoc = DectAssociationData()
            assoc.longRdId = getIntValue(items[idx++]).toUInt()
            assoc.localIpv6Addr = decodeIpv6Addr(items[idx++])
            assoc.globalIpv6AddrSet = decodeBoolean(items[idx++])
            if (assoc.globalIpv6AddrSet) {
                assoc.globalIpv6Addr = decodeIpv6Addr(items[idx++])
            }
            assoc
        }

        // Border router info
        status.brNetIfaceIndex = getIntValue(items[idx++])
        status.brGlobalIpv6AddrPrefixSet = decodeBoolean(items[idx++])
        if (status.brGlobalIpv6AddrPrefixSet) {
            status.brGlobalIpv6AddrPrefix = decodeIpv6Addr(items[idx++])
            status.brGlobalIpv6AddrPrefixLen = getIntValue(items[idx++])
        }

        // Firmware version string
        val fwVersionObj = items[idx++]
        if (fwVersionObj is SimpleValue && fwVersionObj.simpleValueType == SimpleValueType.NULL) {
            status.fwVersionStr = ""
        } else if (fwVersionObj is UnicodeString) {
            status.fwVersionStr = fwVersionObj.getString()
        }

        return status
    }

    /**
     * Decode boolean (handles SimpleValue and UnicodeString for compatibility)
     */
    private fun decodeBoolean(obj: DataItem): Boolean {
        return when (obj) {
            is UnicodeString -> obj.getString() == "true"
            is SimpleValue -> when (obj.simpleValueType) {
                SimpleValueType.TRUE -> true
                SimpleValueType.FALSE -> false
                else -> false
            }
            else -> false
        }
    }

    /**
     * Helper to extract integer value from DataItem (handles UnsignedInteger, NegativeInteger, etc.)
     */
    private fun getIntValue(obj: DataItem): Int {
        return when (obj) {
            is UnsignedInteger -> obj.value.toInt()
            is NegativeInteger -> (-obj.value.toInt())
            is co.nstant.`in`.cbor.model.Number -> obj.value.toInt()
            else -> throw IllegalArgumentException("Expected number type, got ${obj.javaClass.simpleName}")
        }
    }

    /**
     * Helper to extract long value from DataItem
     */
    private fun getLongValue(obj: DataItem): Long {
        return when (obj) {
            is UnsignedInteger -> obj.value.toLong()
            is NegativeInteger -> (-obj.value.toLong())
            is co.nstant.`in`.cbor.model.Number -> obj.value.toLong()
            else -> throw IllegalArgumentException("Expected number type, got ${obj.javaClass.simpleName}")
        }
    }

    /**
     * Decode IPv6 address
     */
    private fun decodeIpv6Addr(obj: DataItem): Inet6Address? {
        if (obj is SimpleValue && obj.simpleValueType == SimpleValueType.NULL) {
            return null
        }

        val bytes = (obj as ByteString).bytes
        if (bytes.size == 16) {
            return InetAddress.getByAddress(bytes) as? Inet6Address
        }

        return null
    }

    /**
     * Decode event data
     * C code encodes event as uint64_t, we decode as ULong
     */
    fun decodeEvent(cborData: ByteArray): EventInfo {
        val obj = decodeCbor(cborData)
        val items = if (obj is Array) obj.getDataItems() else throw IllegalStateException("Invalid event format")
        if (items.size < 2) {
            throw IllegalStateException("Invalid event format")
        }

        val ifaceIndex = getIntValue(items[0])
        // Decode as uint64 (ULong) to match C encoding
        val eventId = getLongValue(items[1]).toULong()
        val data = if (items.size > 2) {
            (items[2] as ByteString).bytes
        } else {
            null
        }

        return EventInfo(ifaceIndex, eventId, data)
    }

    /**
     * Decode CBOR bytes to DataItem
     */
    private fun decodeCbor(data: ByteArray): DataItem {
        val bais = ByteArrayInputStream(data)
        val decoder = CborDecoder(bais)
        val dataItems = decoder.decode()
        if (dataItems.isEmpty()) {
            throw IllegalArgumentException("Empty CBOR data")
        }
        return dataItems[0]
    }

    /**
     * Encode CBOR DataItem to bytes
     */
    private fun encodeCbor(dataItem: DataItem): ByteArray {
        val baos = ByteArrayOutputStream()
        CborEncoder(baos).encode(dataItem)
        return baos.toByteArray()
    }

    /**
     * Event information
     */
    data class EventInfo(
        val ifaceIndex: Int,
        val eventId: ULong,
        val data: ByteArray?
    )
}


