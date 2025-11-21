/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc_shell.services

import com.google.gson.Gson
import com.google.gson.JsonObject
import com.google.gson.JsonParser
import com.nordicsemi.dect_rpc_shell.models.CloudDevice
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import java.text.SimpleDateFormat
import java.util.Locale
import java.util.concurrent.TimeUnit

/**
 * nRF Cloud REST API service
 */
object CloudRestService {
    private const val API_BASE_URL = "https://api.nrfcloud.com/v1"
    
    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()
    
    private val gson = Gson()
    private val dateFormat = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US)

    /**
     * Fetch list of devices from nRF Cloud
     */
    suspend fun getDevices(apiKey: String): List<CloudDevice> = withContext(Dispatchers.IO) {
        require(apiKey.isNotBlank()) { "API key cannot be null or empty" }

        val request = Request.Builder()
            .url("$API_BASE_URL/devices")
            .addHeader("Authorization", "Bearer $apiKey")
            .addHeader("Accept", "application/json")
            .get()
            .build()

        val response = httpClient.newCall(request).execute()
        
        if (!response.isSuccessful) {
            throw Exception("Failed to fetch devices: ${response.code} ${response.message}")
        }

        val jsonContent = response.body?.string() ?: throw Exception("Empty response body")
        val devices = parseDevicesJson(jsonContent)
        
        // For devices without state object, try fetching individual device details
        // (the list endpoint may not return full state for all devices)
        val devicesWithDetails = mutableListOf<CloudDevice>()
        for (device in devices) {
            if (device.online == false && device.connectionStatus == null) {
                // Try to fetch individual device details to get state information
                try {
                    android.util.Log.d("CloudRestService", "Fetching individual device details for ${device.id}")
                    val detailedDevice = getDeviceDetails(apiKey, device.id)
                    devicesWithDetails.add(detailedDevice ?: device)
                } catch (e: Exception) {
                    android.util.Log.d("CloudRestService", "Failed to fetch device details for ${device.id}: ${e.message}")
                    devicesWithDetails.add(device)
                }
            } else {
                devicesWithDetails.add(device)
            }
        }
        
        return@withContext devicesWithDetails
    }
    
    /**
     * Fetch individual device details from nRF Cloud
     * This endpoint returns full device information including state object
     */
    private suspend fun getDeviceDetails(apiKey: String, deviceId: String): CloudDevice? = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url("$API_BASE_URL/devices/$deviceId")
                .addHeader("Authorization", "Bearer $apiKey")
                .addHeader("Accept", "application/json")
                .get()
                .build()

            val response = httpClient.newCall(request).execute()
            
            if (!response.isSuccessful) {
                android.util.Log.d("CloudRestService", "Failed to fetch device details: ${response.code} ${response.message}")
                return@withContext null
            }

            val jsonContent = response.body?.string() ?: return@withContext null
            
            // Parse the individual device JSON (it should have the same structure as list items)
            val json = JsonParser.parseString(jsonContent).asJsonObject
            val devices = parseDevicesJson("{\"items\":[$json]}")
            
            return@withContext devices.firstOrNull()
        } catch (e: Exception) {
            android.util.Log.e("CloudRestService", "Error fetching device details: ${e.message}", e)
            return@withContext null
        }
    }

    /**
     * Validate API key by making a test request
     */
    suspend fun validateApiKey(apiKey: String): Boolean = withContext(Dispatchers.IO) {
        if (apiKey.isBlank()) {
            return@withContext false
        }

        try {
            val request = Request.Builder()
                .url("$API_BASE_URL/devices?limit=1")
                .addHeader("Authorization", "Bearer $apiKey")
                .addHeader("Accept", "application/json")
                .get()
                .build()

            val response = httpClient.newCall(request).execute()
            return@withContext response.isSuccessful
        } catch (e: Exception) {
            return@withContext false
        }
    }

    private fun parseDevicesJson(jsonContent: String): List<CloudDevice> {
        val devices = mutableListOf<CloudDevice>()
        val json = JsonParser.parseString(jsonContent).asJsonObject

        // Log the raw JSON structure for first device to understand API response format
        android.util.Log.d("CloudRestService", "=== Parsing devices JSON ===")
        
        if (json.has("items") && json.get("items").isJsonArray) {
            val items = json.getAsJsonArray("items")
            android.util.Log.d("CloudRestService", "Found ${items.size()} devices")
            
            items.forEach { item ->
                val itemObj = item.asJsonObject
                val deviceId = itemObj.get("id")?.asString ?: "Unknown"
                
                // Log full device JSON structure for debugging (split into chunks to avoid truncation)
                android.util.Log.d("CloudRestService", "=== Device: $deviceId ===")
                val fullJson = itemObj.toString()
                // Split long JSON into chunks (Android logcat limit is ~4KB)
                val chunkSize = 3000
                if (fullJson.length > chunkSize) {
                    var offset = 0
                    var chunkNum = 0
                    while (offset < fullJson.length) {
                        val chunk = fullJson.substring(offset, minOf(offset + chunkSize, fullJson.length))
                        android.util.Log.d("CloudRestService", "Full device JSON [chunk $chunkNum]: $chunk")
                        offset += chunkSize
                        chunkNum++
                    }
                } else {
                    android.util.Log.d("CloudRestService", "Full device JSON: $fullJson")
                }
                
                // Parse state information (like reference project)
                // According to nRF Cloud API: https://api.nrfcloud.com/v1/#tag/All-Devices/operation/ListDevices
                // Note: Devices that haven't connected yet may not have a "state" object
                val stateObj = itemObj.get("state")?.asJsonObject
                if (stateObj == null) {
                    android.util.Log.d("CloudRestService", "Device $deviceId - No state object (device may not be connected/onboarded yet)")
                    android.util.Log.d("CloudRestService", "Device $deviceId - subType: ${itemObj.get("subType")?.asString}")
                } else {
                    android.util.Log.d("CloudRestService", "Device $deviceId - state object exists")
                }
                
                val reportedObj = stateObj?.get("reported")?.asJsonObject
                if (reportedObj != null) {
                    android.util.Log.d("CloudRestService", "Device $deviceId - reported object exists")
                }
                
                // Extract connection info from state.reported.device.connectionInfo (like reference project)
                val deviceObj = reportedObj?.get("device")?.asJsonObject
                val connectionInfoObj = deviceObj?.get("connectionInfo")?.asJsonObject
                val protocol = connectionInfoObj?.get("protocol")?.asString
                val method = connectionInfoObj?.get("method")?.asString
                android.util.Log.d("CloudRestService", "Device $deviceId - protocol: $protocol, method: $method")
                
                // Extract connection status from state.reported.connection.status (like reference project)
                // This is the primary indicator of online status
                val connectionObj = reportedObj?.get("connection")?.asJsonObject
                android.util.Log.d("CloudRestService", "Device $deviceId - reported.connection object: ${connectionObj?.toString()}")
                
                val connectionStatus = connectionObj?.get("status")?.asString
                android.util.Log.d("CloudRestService", "Device $deviceId - reported.connection.status: $connectionStatus")
                
                // Extract device info from state.reported.device.deviceInfo (like reference project)
                val deviceInfoObj = deviceObj?.get("deviceInfo")?.asJsonObject
                // batteryVoltage is in mV (Int), convert to V (Double)
                val batteryVoltage = deviceInfoObj?.get("batteryVoltage")?.asInt?.toDouble()?.div(1000.0)
                val board = deviceInfoObj?.get("board")?.asString
                val hardwareVersion = deviceInfoObj?.get("hwVer")?.asString
                android.util.Log.d("CloudRestService", "Device $deviceId - board: $board, hwVer: $hardwareVersion, batteryVoltage: $batteryVoltage")
                
                // Extract firmware info
                val firmwareObj = itemObj.get("firmware")?.asJsonObject
                val appObj = firmwareObj?.get("app")?.asJsonObject
                val firmware = CloudDevice.FirmwareInfo(
                    appName = appObj?.get("name")?.asString,
                    appVersion = appObj?.get("version")?.asString,
                    modemVersion = firmwareObj?.get("modem")?.asString
                )
                
                // Determine online status according to nRF Cloud API structure (like reference project)
                // According to reference project: Device is online if state.reported.connection.status == "connected"
                // If no state object exists, device is definitely offline (not connected/onboarded)
                val isOnline = when {
                    stateObj == null -> {
                        // No state object means device hasn't connected to nRF Cloud yet
                        val subType = itemObj.get("subType")?.asString
                        android.util.Log.d("CloudRestService", "Device ${itemObj.get("id")?.asString} is offline: No state object (subType=$subType, device not connected/onboarded)")
                        false
                    }
                    connectionStatus == "connected" -> {
                        android.util.Log.d("CloudRestService", "Device ${itemObj.get("id")?.asString} is online: connection.status=connected")
                        true
                    }
                    connectionStatus == "disconnected" -> {
                        android.util.Log.d("CloudRestService", "Device ${itemObj.get("id")?.asString} is offline: connection.status=disconnected")
                        false
                    }
                    else -> {
                        // Connection status is null or unknown - device is offline
                        android.util.Log.d("CloudRestService", "Device ${itemObj.get("id")?.asString} is offline: connection.status=$connectionStatus (null or unknown)")
                        false
                    }
                }
                
                // Extract tenantId from device object
                // TenantId is typically at the root level of the device object
                val tenantId = itemObj.get("tenantId")?.asString
                android.util.Log.d("CloudRestService", "Device ${itemObj.get("id")?.asString}: tenantId=$tenantId")
                
                // Debug logging to help diagnose issues
                android.util.Log.d("CloudRestService", "Device ${itemObj.get("id")?.asString}: connectionStatus=$connectionStatus, isOnline=$isOnline")
                
                val device = CloudDevice(
                    id = itemObj.get("id")?.asString ?: "Unknown",
                    name = itemObj.get("name")?.asString ?: itemObj.get("id")?.asString ?: "Unknown Device",
                    type = itemObj.get("type")?.asString,
                    online = isOnline,
                    lastSeen = stateObj?.get("lastSeen")?.asString?.let { dateStr ->
                        try {
                            dateFormat.parse(dateStr)
                        } catch (e: Exception) {
                            null
                        }
                    },
                    protocol = protocol,
                    connectionStatus = connectionStatus,
                    method = method,
                    firmware = firmware,
                    batteryVoltage = batteryVoltage,
                    board = board,
                    hardwareVersion = hardwareVersion,
                    tenantId = tenantId
                )
                devices.add(device)
            }
        }

        return devices
    }
}


