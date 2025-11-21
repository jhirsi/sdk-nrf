/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc

import co.nstant.`in`.cbor.model.DataItem
import com.google.gson.JsonObject
import com.google.gson.JsonParser
import kotlin.coroutines.Continuation
import kotlinx.coroutines.*
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException
import kotlin.coroutines.suspendCoroutine

/**
 * Pending command information
 */
private data class PendingCommand(
    val commandId: Byte,
    val contextId: Byte,
    val continuation: Continuation<ByteArray>
)

/**
 * DECT RPC client for cloud applications
 * 
 * Uses REST API instead of MQTT since MQTT requires device certificates (mTLS).
 * REST API supports API key authentication.
 * 
 * Note: REST API doesn't support real-time bidirectional communication like MQTT.
 * Responses must be polled or handled differently.
 * 
 * See: https://api.nrfcloud.com/v1/#tag/Messages
 */
class DectRpcClient : AutoCloseable {
    private val API_BASE_URL = "https://api.nrfcloud.com/v1"
    
    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()
    
    private var deviceId: String? = null
    private var apiKey: String? = null
    private var tenantId: String? = null // Store tenantId from first message response
    private var connected = false
    private val commandMutex = Mutex()
    private val pendingCommands = ConcurrentHashMap<Byte, PendingCommand>()
    private var nextContextId: Byte = 1 // Start from 1, 0 is reserved
    private val MAX_CONTEXT_ID: Byte = 0x7F // 7 bits max
    private var disposed = false
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var lastSeenReceivedAt: String? = null // Track the latest receivedAt timestamp from messages we've seen
    private var commandSentTimestamp: String? = null // Track when command was sent (for start parameter)
    
    /**
     * Polling configuration
     */
    var pollingTimeoutMs: Long = 10000 // Default 10 seconds (reduced for faster response)
    var pollingIntervalMs: Long = 500  // Default 500ms between polls (faster polling)
    var maxPollAttempts: Int = 20      // Default 20 attempts
    var emptyResponseCount: Int = 2    // Number of consecutive empty responses before considering complete

    /**
     * Connection state
     */
    val isConnected: Boolean
        get() = connected && deviceId != null && apiKey != null

    /**
     * Connection state changed event
     */
    var onConnectionStateChanged: ((Boolean) -> Unit)? = null

    /**
     * DECT RPC event received
     */
    var onEventReceived: ((DectRpcEvent) -> Unit)? = null

    /**
     * Connect to nRF Cloud REST API
     * 
     * @param deviceId Device ID
     * @param apiKey API key for authentication
     */
    suspend fun connect(deviceId: String, apiKey: String): Unit = withContext(Dispatchers.IO) {
        if (disposed) {
            throw IllegalStateException("Client is disposed")
        }

        this@DectRpcClient.deviceId = deviceId
        this@DectRpcClient.apiKey = apiKey

        // REST API doesn't require a persistent connection like MQTT
        // Just initialize the client
        android.util.Log.d("DectRpcClient", "=== Initializing REST API client ===")
        android.util.Log.d("DectRpcClient", "Device ID: $deviceId")
        android.util.Log.d("DectRpcClient", "API Key length: ${apiKey.length}")

        this@DectRpcClient.connected = true
            onConnectionStateChanged?.invoke(true)
        
        android.util.Log.d("DectRpcClient", "=== REST API client ready ===")
    }

    /**
     * Disconnect from nRF Cloud (no-op for REST API, kept for API compatibility)
     */
    suspend fun disconnect() {
        android.util.Log.d("DectRpcClient", "Disconnect called (no-op for REST API)")
        connected = false
            onConnectionStateChanged?.invoke(false)
    }

    /**
     * Activate DECT interface
     */
    suspend fun activate(ifaceIndex: Int): Int {
        return sendCommand(DectRpcCmdServer.ACTIVATE.value, CborSerializer.encodeIfaceIndex(ifaceIndex))
    }

    /**
     * Deactivate DECT interface
     */
    suspend fun deactivate(ifaceIndex: Int): Int {
        return sendCommand(DectRpcCmdServer.DEACTIVATE.value, CborSerializer.encodeIfaceIndex(ifaceIndex))
    }

    /**
     * Get DECT status information
     */
    suspend fun getStatusInfo(ifaceIndex: Int): DectStatusInfo {
        val payload = sendCommandWithResponse(
            DectRpcCmdServer.STATUS_INFO_GET.value,
            CborSerializer.encodeIfaceIndex(ifaceIndex)
        )

        if (payload.isEmpty()) {
            throw IllegalStateException("Failed to get status info")
        }

        // Response format: [result_code (int), status_info (CBOR array)]
        // Extract the status_info part (skip result_code)
        return CborSerializer.decodeStatusInfoFromResponse(payload)
    }

    /**
     * Read DECT settings
     */
    suspend fun readSettings(ifaceIndex: Int): DectSettings {
        val payload = sendCommandWithResponse(
            DectRpcCmdServer.SETTINGS_READ.value,
            CborSerializer.encodeIfaceIndex(ifaceIndex)
        )

        if (payload.isEmpty()) {
            throw IllegalStateException("Failed to read settings")
        }

        // Response format: [result_code (int), settings (CBOR array)]
        // Extract the settings part (skip result_code)
        return CborSerializer.decodeSettingsFromResponse(payload)
    }

    /**
     * Write DECT settings
     */
    suspend fun writeSettings(ifaceIndex: Int, settings: DectSettings): Int {
        val cborPayload = CborSerializer.encodeSettings(settings)
        val array = co.nstant.`in`.cbor.model.Array()
        array.add(co.nstant.`in`.cbor.model.UnsignedInteger(ifaceIndex.toLong()))
        array.add(cborPayload)
        
        return sendCommand(DectRpcCmdServer.SETTINGS_WRITE.value, array)
    }

    /**
     * Subscribe to DECT events
     */
    suspend fun subscribeEvents(ifaceIndex: Int, eventMask: UInt): Int {
        val array = co.nstant.`in`.cbor.model.Array()
        array.add(co.nstant.`in`.cbor.model.UnsignedInteger(ifaceIndex.toLong()))
        array.add(co.nstant.`in`.cbor.model.UnsignedInteger(eventMask.toLong()))
        
        return sendCommand(DectRpcCmdServer.EVENT_SUBSCRIBE.value, array)
    }

    /**
     * Send command and wait for response
     */
    private suspend fun sendCommand(cmdId: Byte, cborPayload: DataItem): Int {
        val payload = sendCommandWithResponse(cmdId, cborPayload)
        if (payload.isEmpty()) {
            return -1
        }
        
        // Response format: [result_code (int), ...optional data...]
        val result = CborSerializer.decodeIntResult(payload)
        return result
    }

    /**
     * Send command and wait for response payload
     * Returns the full CBOR response payload (including result code)
     * 
     * Note: REST API doesn't support real-time responses. This implementation
     * sends the command but cannot receive responses in real-time.
     */
    private suspend fun sendCommandWithResponse(cmdId: Byte, cborPayload: DataItem): ByteArray = withContext(Dispatchers.IO) {
        if (!isConnected) {
            throw IllegalStateException("Not connected to REST API")
        }

        return@withContext commandMutex.withLock {
            // Allocate context ID
            val contextId = nextContextId
            nextContextId++
            if (nextContextId > MAX_CONTEXT_ID) {
                nextContextId = 1 // Wrap around
            }

                // Store command timestamp before sending (for start parameter in polling)
                // Reset lastSeenReceivedAt to null so we use commandSentTimestamp for this command's polling
                commandSentTimestamp = getIso8601Timestamp()
                lastSeenReceivedAt = null  // Reset so we use commandSentTimestamp for this command
                android.util.Log.d("DectRpcClient", "Command sent timestamp: $commandSentTimestamp (will use this for start parameter, reset lastSeenReceivedAt)")

                try {
                    // Encode command with context ID
                    val packet = NrfRpcProtocol.encodeCommand(cmdId, contextId, cborPayload)

                // Log packet details for debugging
                android.util.Log.d("DectRpcClient", "Encoded packet: len=${packet.size}, cmdId=$cmdId, contextId=$contextId")
                android.util.Log.d("DectRpcClient", "Packet hex: ${packet.joinToString(" ") { "%02X".format(it) }}")
                if (packet.size >= 5) {
                    android.util.Log.d("DectRpcClient", "Header: [0]=0x%02X, [1]=0x%02X (cmdId), [2]=0x%02X, [3]=0x%02X, [4]=0x%02X".format(
                        packet[0].toInt() and 0xFF, packet[1].toInt() and 0xFF, packet[2].toInt() and 0xFF,
                        packet[3].toInt() and 0xFF, packet[4].toInt() and 0xFF))
                    if (packet.size > 5) {
                        android.util.Log.d("DectRpcClient", "Payload (${packet.size - 5} bytes): ${packet.sliceArray(5 until packet.size).joinToString(" ") { "%02X".format(it) }}")
                    }
                }

                // Convert packet to base64 for JSON transport
                val base64Packet = android.util.Base64.encodeToString(packet, android.util.Base64.NO_WRAP)

                // Create JSON message: {"appId":"DECT_RPC", "data":"<base64_packet>"}
                val messageObj = JsonObject().apply {
                    addProperty("appId", "DECT_RPC")
                    addProperty("data", base64Packet)
                }

                // Create request body according to nRF Cloud REST API Messages endpoint
                // POST /v1/devices/{deviceId}/messages
                // Body: {"topic": "d/{deviceId}/c2d", "message": {"appId": "...", "data": "..."}}
                // Using /c2d topic (same as DirectShellClient) - device subscribes to pattern matching /+/r
                val requestBody = JsonObject().apply {
                    addProperty("topic", "d/${this@DectRpcClient.deviceId}/c2d")
                    add("message", messageObj)
                }

                val gson = com.google.gson.Gson()
                val jsonString = gson.toJson(requestBody)
                android.util.Log.d("DectRpcClient", "Sending RPC command: cmdId=$cmdId, contextId=$contextId")

                val requestBodyObj = jsonString.toRequestBody("application/json".toMediaType())

                // POST to /v1/devices/{deviceId}/messages
                val url = "$API_BASE_URL/devices/${this@DectRpcClient.deviceId}/messages"
                val request = Request.Builder()
                    .url(url)
                    .addHeader("Authorization", "Bearer ${this@DectRpcClient.apiKey}")
                    .addHeader("Content-Type", "application/json")
                    .addHeader("Connection", "close")
                    .post(requestBodyObj)
                    .build()

                android.util.Log.d("DectRpcClient", "POST $url")
                val response = httpClient.newCall(request).execute()

                if (response.isSuccessful) {
                    android.util.Log.d("DectRpcClient", "Command sent successfully")
                    val responseBody = response.body?.string()
                    android.util.Log.d("DectRpcClient", "Response: $responseBody")

                    // Poll for response with timeout
                    return@withContext pollForResponse(contextId)
                } else {
                    android.util.Log.e("DectRpcClient", "Failed to send command: ${response.code} ${response.message}")
                    val errorBody = response.body?.string()
                    android.util.Log.e("DectRpcClient", "Error response: $errorBody")
                    throw IllegalStateException("Failed to send command: ${response.code}")
                }
            } catch (e: Exception) {
                android.util.Log.e("DectRpcClient", "Error sending command: ${e.message}", e)
                throw e
            }
        }
    }

    /**
     * Poll for RPC response matching the given context ID
     * Returns the response payload or empty array if timeout/not found
     */
    private suspend fun pollForResponse(contextId: Byte): ByteArray = withContext(Dispatchers.IO) {
        val startTime = System.currentTimeMillis()
        var attempt = 0
        
        android.util.Log.d("DectRpcClient", "Starting polling for contextId=$contextId: timeout=${pollingTimeoutMs}ms, interval=${pollingIntervalMs}ms, maxAttempts=$maxPollAttempts")
        
        while (attempt < maxPollAttempts) {
            val elapsed = System.currentTimeMillis() - startTime
            
            // Check timeout
            if (elapsed >= pollingTimeoutMs) {
                android.util.Log.d("DectRpcClient", "Polling timeout reached for contextId=$contextId (${elapsed}ms)")
                break
            }
            
            try {
                attempt++
                android.util.Log.d("DectRpcClient", "Polling attempt $attempt/$maxPollAttempts for contextId=$contextId (elapsed: ${elapsed}ms)")
                
                // Poll for responses using ListMessages API
                val matchedResponse = pollForResponses(contextId)
                if (matchedResponse != null) {
                    android.util.Log.d("DectRpcClient", "Found response for contextId=$contextId")
                    return@withContext matchedResponse
                }
                
                // Wait before next poll (unless we've reached timeout or max attempts)
                if (attempt < maxPollAttempts && (System.currentTimeMillis() - startTime) < pollingTimeoutMs) {
                    delay(pollingIntervalMs)
                    }
                } catch (e: Exception) {
                android.util.Log.e("DectRpcClient", "Polling error for contextId=$contextId: ${e.message}", e)
                // Continue polling on error (counts as an attempt)
                if (attempt < maxPollAttempts && (System.currentTimeMillis() - startTime) < pollingTimeoutMs) {
                    delay(pollingIntervalMs)
                }
            }
        }
        
        android.util.Log.w("DectRpcClient", "No response found for contextId=$contextId after $attempt attempts, ${System.currentTimeMillis() - startTime}ms")
        ByteArray(0)
    }
    
    /**
     * Poll for RPC responses using ListMessages API
     * GET /v1/messages to retrieve d2c messages
     * Uses start parameter with timestamp and topic filter with tenantId to get only the most recent messages
     * 
     * @param contextId Context ID to match responses for
     * @return Response payload if found, null otherwise
     */
    private suspend fun pollForResponses(contextId: Byte): ByteArray? = withContext(Dispatchers.IO) {
        if (!isConnected) {
            return@withContext null
        }

        try {
            // Build URL with deviceId, topic filter (if tenantId is known), and start timestamp
            val urlBuilder = StringBuilder("$API_BASE_URL/messages?deviceId=$deviceId")
            
            // Add topic filter if tenantId is known
            // Using /d2c topic (message topic) to match NRF_CLOUD_TOPIC_MESSAGE
            // Format: prod/{tenantId}/m/d/{deviceId}/d2c
            // This matches where dect_shell RPC config sends responses via NRF_CLOUD_TOPIC_MESSAGE
            if (tenantId != null && deviceId != null) {
                val topicFilter = "prod/$tenantId/m/d/$deviceId/d2c"
                urlBuilder.append("&topic=").append(java.net.URLEncoder.encode(topicFilter, "UTF-8"))
                android.util.Log.d("DectRpcClient", "Polling from MESSAGE topic: $topicFilter")
            } else {
                android.util.Log.d("DectRpcClient", "No tenantId yet, polling without topic filter (will extract tenantId from first message)")
            }
            
            // Use timestamp for start parameter:
            // - If we have commandSentTimestamp, use that minus 5 seconds (to catch response that arrives quickly)
            // - If we have seen messages in this polling session, use lastSeenReceivedAt + 1 second
            // - Otherwise, use current time minus 30 seconds in UTC (Z time)
            // Note: We prioritize commandSentTimestamp to ensure we catch the response for this specific command
            val startTime = if (commandSentTimestamp != null) {
                // Use command sent time minus 5 seconds to ensure we catch the response
                // Reduced from 10 seconds for faster polling
                addSecondsToIso8601Timestamp(commandSentTimestamp!!, -5)
            } else if (lastSeenReceivedAt != null) {
                // We've seen messages, use last seen + 1 second
                addSecondsToIso8601Timestamp(lastSeenReceivedAt!!, 1)
            } else {
                // Fallback: use current time minus 30 seconds (reduced from 60)
                val cal = java.util.Calendar.getInstance(java.util.TimeZone.getTimeZone("UTC"))
                cal.add(java.util.Calendar.SECOND, -30)
                getIso8601Timestamp(cal.time)
            }
            urlBuilder.append("&start=").append(java.net.URLEncoder.encode(startTime, "UTF-8"))
            android.util.Log.d("DectRpcClient", "Using start parameter: $startTime (lastSeenReceivedAt=${lastSeenReceivedAt})")
            
            val url = urlBuilder.toString()
            android.util.Log.d("DectRpcClient", "Polling messages: $url")
            
            val request = Request.Builder()
                .url(url)
                .addHeader("Authorization", "Bearer $apiKey")
                .addHeader("Content-Type", "application/json")
                .addHeader("Connection", "close")
                .get()
                .build()

            val response = httpClient.newCall(request).execute()

            if (response.isSuccessful) {
                val responseBody = response.body?.string()
                if (responseBody != null && responseBody.isNotEmpty()) {
                    android.util.Log.d("DectRpcClient", "Received messages response (${responseBody.length} chars): $responseBody")
                    android.util.Log.d("DectRpcClient", "Polling URL was: $url")
                    
                    // Parse messages response
                    try {
                        val json = JsonParser.parseString(responseBody).asJsonObject
                        
                        // Check for "items" array (nRF Cloud messages API format)
                        if (json.has("items") && json.get("items").isJsonArray) {
                            val items = json.get("items").asJsonArray
                            android.util.Log.d("DectRpcClient", "Found ${items.size()} items in response (looking for contextId=$contextId)")
                            
                            // Log first few items for debugging
                            if (items.size() > 0) {
                                android.util.Log.d("DectRpcClient", "First item: ${items[0]}")
                            }
                            
                            for (itemElement in items) {
                                if (itemElement.isJsonObject) {
                                    val item = itemElement.asJsonObject
                                    
                                    // Extract and store tenantId from first message if not already stored
                                    // This tenantId is used to construct the same topic filter as DirectShellClient
                                    if (tenantId == null && item.has("tenantId")) {
                                        tenantId = item.get("tenantId").asString
                                        android.util.Log.d("DectRpcClient", "Extracted tenantId from message: $tenantId (same as DirectShellClient uses)")
                                    }
                                    
                                    // Extract receivedAt timestamp
                                    val receivedAt = if (item.has("receivedAt")) item.get("receivedAt").asString else null
                                    if (receivedAt != null) {
                                        updateLastSeenReceivedAt(receivedAt)
                                    }
                                    
                                    // Check topic - should end with /d/{deviceId}/d2c (device-to-cloud message topic)
                                    // This is the same topic format that dect_shell RPC config uses for responses via NRF_CLOUD_TOPIC_MESSAGE
                                    val topic = if (item.has("topic")) item.get("topic").asString else ""
                                    val expectedSuffix = "/d/$deviceId/d2c"
                                    val isD2cTopic = topic.endsWith(expectedSuffix)
                                    
                                    android.util.Log.d("DectRpcClient", "Checking message topic: $topic (matches d2c: $isD2cTopic)")

                                    if (isD2cTopic) {
                                        // Device sends responses as raw binary via MQTT (no JSON/appId wrapper)
                                        // REST API might return them in different formats:
                                        // 1. JSON-wrapped: {"message": {"appId": "DECT_RPC", "data": "<base64>"}}
                                        // 2. Raw binary as base64: {"data": "<base64>"} (no appId)
                                        // 3. Direct binary payload
                                        
                                        var packet: ByteArray? = null
                                        
                                        // Format 1: Check if item has "message" field containing JSON-wrapped message
                                        if (item.has("message") && item.get("message").isJsonObject) {
                                            val message = item.get("message").asJsonObject
                                            
                                            // Check for DECT_RPC messages (if JSON-wrapped)
                                            val appId = if (message.has("appId")) message.get("appId").asString else ""
                                            android.util.Log.d("DectRpcClient", "Message appId: $appId")
                                            
                                               if (appId == "DECT_RPC" && message.has("data")) {
                                                   android.util.Log.d("DectRpcClient", "Found DECT_RPC message on d2c topic (JSON-wrapped)")
                                                   val base64Data = message.get("data").asString
                                                   packet = android.util.Base64.decode(base64Data, android.util.Base64.DEFAULT)
                                               }
                                           }

                                           // Format 2: Check for raw binary data (device sends raw binary via MQTT on MESSAGE topic)
                                           // REST API might return it as base64-encoded data without appId
                                           // This is the most likely format since device sends raw binary via nrf_cloud_send() with NRF_CLOUD_TOPIC_MESSAGE
                                           if (packet == null && item.has("data") && item.get("data").isJsonPrimitive) {
                                               try {
                                                   val dataStr = item.get("data").asString
                                                   // Try to decode as base64 (raw binary MQTT response)
                                                   packet = android.util.Base64.decode(dataStr, android.util.Base64.DEFAULT)
                                                   android.util.Log.d("DectRpcClient", "Found raw binary MQTT message on d2c topic (${packet.size} bytes, no appId - sent via nrf_cloud_send() with NRF_CLOUD_TOPIC_MESSAGE)")
                                               } catch (e: Exception) {
                                                   android.util.Log.d("DectRpcClient", "Data field is not base64: ${e.message}")
                                               }
                                           }
                                        
                                        // Format 3: Check if message is directly in item (fallback)
                                        if (packet == null && item.has("message") && item.get("message").isJsonPrimitive) {
                                            try {
                                                val messageStr = item.get("message").asString
                                                packet = android.util.Base64.decode(messageStr, android.util.Base64.DEFAULT)
                                                android.util.Log.d("DectRpcClient", "Found raw binary message in message field (${packet.size} bytes)")
                                            } catch (e: Exception) {
                                                // Not base64, skip
                                            }
                                        }
                                        
                                        // If we found a packet, try to parse it as RPC response
                                        if (packet != null) {
                                            android.util.Log.d("DectRpcClient", "Decoded RPC packet: len=${packet.size}, hex=${packet.take(20).joinToString(" ") { "%02X".format(it) }}${if (packet.size > 20) "..." else ""}")
                                            
                                            // Parse packet and match by context ID
                                            val matchedResponse = parseRpcResponse(packet, contextId)
                                            if (matchedResponse != null) {
                                                android.util.Log.d("DectRpcClient", "Found RPC response for contextId=$contextId, payload len=${matchedResponse.size}")
                                                return@withContext matchedResponse
                                            } else {
                                                android.util.Log.d("DectRpcClient", "RPC packet did not match contextId=$contextId (might be for different context or not a response)")
                                            }
                                        } else {
                                            android.util.Log.d("DectRpcClient", "No packet data found in d2c message (topic: $topic)")
                                        }
                                    }
                                }
                            }
                        }
                    } catch (e: Exception) {
                        android.util.Log.e("DectRpcClient", "Error parsing messages: ${e.message}", e)
                    }
                }
            } else {
                android.util.Log.d("DectRpcClient", "Messages check failed: ${response.code}")
            }
        } catch (e: Exception) {
            android.util.Log.e("DectRpcClient", "Error polling for responses: ${e.message}", e)
        }
        
        return@withContext null
    }
    
    /**
     * Parse RPC response packet and match by context ID
     * Returns the response payload if matched, null otherwise
     */
    private fun parseRpcResponse(packet: ByteArray, contextId: Byte): ByteArray? {
        try {
            if (packet.size < 5) {
                android.util.Log.d("DectRpcClient", "Packet too small: ${packet.size} bytes")
                return null
            }
            
            // Parse packet header
            val packetInfo = NrfRpcProtocol.decodePacket(packet)
            android.util.Log.d("DectRpcClient", "Parsed packet: type=0x%02X, cmdId=0x%02X, srcContextId=0x%02X, dstContextId=0x%02X, payloadLen=${packetInfo.payload.size}".format(
                packetInfo.packetType.toInt() and 0xFF,
                packetInfo.cmdId.toInt() and 0xFF,
                packetInfo.srcContextId.toInt() and 0xFF,
                packetInfo.dstContextId.toInt() and 0xFF
            ))
            
            // Check if this is a response packet (type 0x01) and matches our context ID
            val isResponse = packetInfo.packetType == 0x01.toByte()
            val matchesContext = packetInfo.dstContextId == contextId
            android.util.Log.d("DectRpcClient", "Checking match: isResponse=$isResponse, matchesContext=$matchesContext (looking for contextId=$contextId, got dstContextId=${packetInfo.dstContextId})")
            
            if (isResponse && matchesContext) {
                android.util.Log.d("DectRpcClient", "Response matched! Returning payload (${packetInfo.payload.size} bytes)")
                // Return the payload
                return packetInfo.payload
            }
            
            return null
        } catch (e: Exception) {
            android.util.Log.e("DectRpcClient", "Error parsing RPC response: ${e.message}", e)
            return null
        }
    }
    
    /**
     * Get ISO 8601 formatted timestamp
     */
    private fun getIso8601Timestamp(date: java.util.Date = java.util.Date()): String {
        val sdf = java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", java.util.Locale.US)
        sdf.timeZone = java.util.TimeZone.getTimeZone("UTC")
        return sdf.format(date)
    }

    /**
     * Add seconds to an ISO 8601 timestamp string
     */
    private fun addSecondsToIso8601Timestamp(iso8601Timestamp: String, seconds: Int): String {
        try {
            val sdf = java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", java.util.Locale.US)
            sdf.timeZone = java.util.TimeZone.getTimeZone("UTC")
            val date = sdf.parse(iso8601Timestamp)
            if (date != null) {
                val cal = java.util.Calendar.getInstance(java.util.TimeZone.getTimeZone("UTC"))
                cal.time = date
                cal.add(java.util.Calendar.SECOND, seconds)
                return getIso8601Timestamp(cal.time)
            }
        } catch (e: Exception) {
            android.util.Log.e("DectRpcClient", "Error parsing ISO8601 timestamp: $iso8601Timestamp", e)
        }
        return iso8601Timestamp
    }

    /**
     * Update lastSeenReceivedAt if the new receivedAt is later than the current one
     */
    private fun updateLastSeenReceivedAt(receivedAt: String?) {
        if (receivedAt == null) return
        
        try {
            if (lastSeenReceivedAt == null) {
                lastSeenReceivedAt = receivedAt
            } else {
                val sdf = java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", java.util.Locale.US)
                sdf.timeZone = java.util.TimeZone.getTimeZone("UTC")
                val currentDate = sdf.parse(lastSeenReceivedAt!!)
                val newDate = sdf.parse(receivedAt)
                
                if (currentDate != null && newDate != null && newDate.after(currentDate)) {
                    lastSeenReceivedAt = receivedAt
                }
            }
        } catch (e: Exception) {
            android.util.Log.e("DectRpcClient", "Error comparing timestamps: $receivedAt vs $lastSeenReceivedAt", e)
        }
    }
    
    /**
     * Handle incoming message (not used with REST API)
     * REST API doesn't support real-time message reception
     */
    private fun handleMqttMessage(packet: ByteArray) {
        // Not used with REST API - messages are polled instead
        android.util.Log.w("DectRpcClient", "handleMqttMessage called but REST API uses polling")
    }

    /**
     * Close and dispose resources
     */
    override fun close() {
        if (disposed) {
            return
        }

        connected = false
        deviceId = null
        apiKey = null
        tenantId = null
        lastSeenReceivedAt = null
        commandSentTimestamp = null
        scope.cancel()
        disposed = true
    }

    /**
     * Helper to encode Array to bytes
     */
    private fun co.nstant.`in`.cbor.model.Array.encodeToBytes(): ByteArray {
        val baos = java.io.ByteArrayOutputStream()
        co.nstant.`in`.cbor.CborEncoder(baos).encode(this)
        return baos.toByteArray()
    }
    
}


