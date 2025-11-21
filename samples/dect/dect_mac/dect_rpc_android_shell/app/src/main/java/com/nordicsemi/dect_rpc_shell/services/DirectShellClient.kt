/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc_shell.services

import com.google.gson.Gson
import com.google.gson.JsonObject
import com.google.gson.JsonParser
import kotlinx.coroutines.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.concurrent.TimeUnit
import kotlinx.coroutines.TimeoutCancellationException
import java.text.SimpleDateFormat
import java.util.*
import java.net.URLEncoder

/**
 * Client for sending direct DECT shell commands via nRF Cloud REST API
 * Commands are sent as JSON: {"appId":"DECT_SHELL", "data":"<command>"}
 * 
 * Uses REST API instead of MQTT since MQTT requires device certificates (mTLS).
 * REST API supports API key authentication.
 * 
 * See: https://api.nrfcloud.com/v1/#tag/Messages
 */
class DirectShellClient : AutoCloseable {
    private val API_BASE_URL = "https://api.nrfcloud.com/v1"
    
    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()
    
    private val gson = Gson()
    private var apiKey: String? = null
    private var deviceId: String? = null
    private var tenantId: String? = null // Store tenantId from first message response
    private var isConnected = false
    private var pollingJob: Job? = null
    private val pollingScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var lastCommandTimestamp: String? = null // Track timestamp of last command sent
    private var lastSeenReceivedAt: String? = null // Track the latest receivedAt timestamp from messages we've seen
    private val shownMessageTimestamps = mutableSetOf<String>() // Track receivedAt timestamps of shown messages (persists across commands)
    private val shownMessageHashes = mutableSetOf<String>() // Track message content hashes as fallback for duplicate detection
    
    // Data class to hold messages with their receivedAt timestamp for sorting
    private data class MessageWithTimestamp(val data: String, val receivedAt: String?)
    
    /**
     * Polling configuration
     */
    var pollingTimeoutMs: Long = 180000 // Default 180 seconds (increased for longer responses)
    var pollingIntervalMs: Long = 1000 // Default 1 second between polls (more frequent to catch all messages)
    var maxPollAttempts: Int = 60 // Default 60 attempts (increased for longer responses)
    var initialPollDelayMs: Long = 2000 // Wait 2 seconds before first poll
    var emptyResponseCount: Int = 8 // Number of consecutive empty responses before considering complete (increased to ensure all data received)

    /**
     * Event fired when a message is received from the device
     * Polling mechanism will call this when responses are found
     */
    var onMessageReceived: ((String) -> Unit)? = null

    /**
     * Event fired when connection state changes
     */
    var onConnectionStateChanged: ((Boolean) -> Unit)? = null

    /**
     * Event fired when polling times out (no response received)
     */
    var onPollingTimeout: (() -> Unit)? = null

    /**
     * Event fired when polling completes (empty responses detected)
     * This indicates all data has been received
     */
    var onPollingComplete: (() -> Unit)? = null

    /**
     * Check if client is connected (initialized)
     */
    fun isConnected(): Boolean = isConnected && deviceId != null && apiKey != null

    /**
     * Connect to nRF Cloud REST API
     * For REST API, this just initializes the client (no actual connection needed)
     * @param tenantId Optional tenant ID. If provided, will be used immediately for topic filtering.
     */
    suspend fun connect(deviceId: String, apiKey: String, tenantId: String? = null): Boolean = withContext(Dispatchers.IO) {
        return@withContext try {
            android.util.Log.d("DirectShellClient", "=== Initializing REST API client ===")
            android.util.Log.d("DirectShellClient", "Device ID: $deviceId")
            android.util.Log.d("DirectShellClient", "API Key length: ${apiKey.length}")
            android.util.Log.d("DirectShellClient", "Tenant ID: $tenantId")
            
            this@DirectShellClient.deviceId = deviceId
            this@DirectShellClient.apiKey = apiKey
            // Store tenantId if provided (from device list)
            if (tenantId != null) {
                this@DirectShellClient.tenantId = tenantId
                android.util.Log.d("DirectShellClient", "TenantId set from device list: $tenantId")
            }
            
            // REST API doesn't require a persistent connection like MQTT
            // Just verify we can make a request by checking device exists
            android.util.Log.d("DirectShellClient", "REST API client initialized")
            
            isConnected = true
            onConnectionStateChanged?.invoke(true)
            
            android.util.Log.d("DirectShellClient", "=== REST API client ready ===")
            true
        } catch (ex: Exception) {
            android.util.Log.e("DirectShellClient", "=== Initialization error ===", ex)
            android.util.Log.e("DirectShellClient", "Error type: ${ex.javaClass.simpleName}")
            android.util.Log.e("DirectShellClient", "Error message: ${ex.message}")
            isConnected = false
            false
        }
    }

    /**
     * Send a DECT shell command via REST API
     * 
     * See: https://api.nrfcloud.com/v1/#tag/Messages
     */
    suspend fun sendCommand(command: String): Boolean = withContext(Dispatchers.IO) {
        if (!isConnected()) {
            android.util.Log.e("DirectShellClient", "Client not connected/initialized")
            return@withContext false
        }

        return@withContext try {
            // Create JSON message: {"appId":"DECT_SHELL", "data":"<command>"}
            val messageObj = JsonObject().apply {
                addProperty("appId", "DECT_SHELL")
                addProperty("data", command)
            }

            // Create request body according to nRF Cloud REST API Messages endpoint
            // POST /v1/devices/{deviceId}/messages
            // Body: {"topic": "d/{deviceId}/c2d", "message": {"appId": "...", "data": "..."}}
            val requestBody = JsonObject().apply {
                addProperty("topic", "d/$deviceId/c2d")
                add("message", messageObj)
            }

            val jsonString = gson.toJson(requestBody)
            android.util.Log.d("DirectShellClient", "Sending command: $jsonString")

            val requestBodyObj = jsonString.toRequestBody("application/json".toMediaType())

            // POST to /v1/devices/{deviceId}/messages
            val url = "$API_BASE_URL/devices/$deviceId/messages"
            val request = Request.Builder()
                .url(url)
                .addHeader("Authorization", "Bearer $apiKey")
                .addHeader("Content-Type", "application/json")
                .addHeader("Connection", "close")
                .post(requestBodyObj)
                .build()

            android.util.Log.d("DirectShellClient", "POST $url")
            
            // Make async call (OkHttpClient already has timeout configured)
            val response = httpClient.newCall(request).execute()

            if (response.isSuccessful) {
                android.util.Log.d("DirectShellClient", "Command sent successfully")
                val responseBody = response.body?.string()
                android.util.Log.d("DirectShellClient", "Response: $responseBody")
                
                // Record timestamp when command was sent (ISO 8601 format)
                val timestamp = getIso8601Timestamp()
                lastCommandTimestamp = timestamp
                android.util.Log.d("DirectShellClient", "Command timestamp: $timestamp")
                
                // Don't clear shown message timestamps - keep them to prevent duplicates across commands
                // Only clear hashes for new command (hashes are less reliable)
                shownMessageHashes.clear()
                android.util.Log.d("DirectShellClient", "Cleared shown message hashes for new command (kept timestamps to prevent duplicates)")
                
                // Start limited polling for response in background (non-blocking)
                // Launch in separate coroutine to ensure sendCommand returns immediately
                pollingScope.launch {
                    startLimitedPolling()
                }
                
                return@withContext true
            } else {
                android.util.Log.e("DirectShellClient", "Failed to send command: ${response.code} ${response.message}")
                val errorBody = response.body?.string()
                android.util.Log.e("DirectShellClient", "Error response: $errorBody")
                return@withContext false
            }
        } catch (ex: TimeoutCancellationException) {
            android.util.Log.e("DirectShellClient", "Timeout sending command: ${ex.message}", ex)
            false
        } catch (ex: Exception) {
            android.util.Log.e("DirectShellClient", "Error sending command: ${ex.message}", ex)
            false
        }
    }

    /**
     * Start limited polling for device responses after sending a command
     * Polls a few times with configurable timeout
     * This runs in background and doesn't block the UI
     */
    private fun startLimitedPolling() {
        // Cancel any existing polling job
        stopPolling()
        
        // Launch polling in background coroutine (non-blocking)
        pollingJob = pollingScope.launch {
            val startTime = System.currentTimeMillis()
            var attempt = 0
            var consecutiveEmptyResponses = 0
            var hasReceivedMessages = false
            
            android.util.Log.d("DirectShellClient", "Starting limited polling: timeout=${pollingTimeoutMs}ms, interval=${pollingIntervalMs}ms, maxAttempts=$maxPollAttempts")
            
            try {
                // Wait before first poll
                delay(initialPollDelayMs)
                
                while (isActive && isConnected()) {
                    val elapsed = System.currentTimeMillis() - startTime
                    
                    // Check timeout
                    if (elapsed >= pollingTimeoutMs) {
                        android.util.Log.d("DirectShellClient", "Polling timeout reached (${elapsed}ms)")
                        break
                    }
                    
                    // Check max attempts
                    if (attempt >= maxPollAttempts) {
                        android.util.Log.d("DirectShellClient", "Max polling attempts reached ($attempt)")
                        break
                    }
                    
                    attempt++
                    android.util.Log.d("DirectShellClient", "Polling attempt $attempt/$maxPollAttempts (elapsed: ${elapsed}ms)")
                    
                    // Poll for responses (non-blocking, uses suspend function)
                    val receivedMessages = pollForResponses()
                    
                    if (receivedMessages > 0) {
                        hasReceivedMessages = true
                        consecutiveEmptyResponses = 0 // Reset counter when we get messages
                        android.util.Log.d("DirectShellClient", "Received $receivedMessages messages, resetting empty response counter")
                    } else {
                        consecutiveEmptyResponses++
                        android.util.Log.d("DirectShellClient", "Empty response ($consecutiveEmptyResponses consecutive)")
                        
                        // If we've received messages before and now have consecutive empty responses, consider it complete
                        if (hasReceivedMessages && consecutiveEmptyResponses >= emptyResponseCount) {
                            android.util.Log.d("DirectShellClient", "Polling complete: received messages and ${consecutiveEmptyResponses} consecutive empty responses")
                            onPollingComplete?.invoke()
                            break
                        }
                    }
                    
                    // Wait before next poll (unless we've reached timeout or max attempts)
                    if (attempt < maxPollAttempts && (System.currentTimeMillis() - startTime) < pollingTimeoutMs) {
                        delay(pollingIntervalMs)
                    }
                }
            } catch (e: CancellationException) {
                android.util.Log.d("DirectShellClient", "Polling cancelled")
                throw e // Re-throw cancellation
            } catch (e: Exception) {
                android.util.Log.e("DirectShellClient", "Polling error: ${e.message}", e)
            } finally {
                val elapsed = System.currentTimeMillis() - startTime
                android.util.Log.d("DirectShellClient", "Polling completed: $attempt attempts, ${elapsed}ms elapsed, receivedMessages=$hasReceivedMessages")
                
                // If we completed polling without finding a response, fire timeout event
                if (!hasReceivedMessages && (attempt >= maxPollAttempts || elapsed >= pollingTimeoutMs)) {
                    android.util.Log.w("DirectShellClient", "Polling timeout - no response received")
                    onPollingTimeout?.invoke()
                } else if (hasReceivedMessages && consecutiveEmptyResponses < emptyResponseCount) {
                    // We received messages but didn't get enough empty responses - might still be receiving
                    // Fire complete anyway after timeout/attempts
                    android.util.Log.d("DirectShellClient", "Polling ended: received messages but didn't get enough empty responses")
                    onPollingComplete?.invoke()
                }
                
                pollingJob = null
            }
        }
    }

    /**
     * Stop polling for responses
     */
    private fun stopPolling() {
        pollingJob?.cancel()
        pollingJob = null
    }

    /**
     * Poll for device responses by checking messages endpoint
     * GET /v1/messages to retrieve d2c messages
     * Uses start parameter with timestamp and topic filter with tenantId to get only the most recent messages
     * 
     * The start parameter filters messages to only those received after the specified timestamp,
     * ensuring we only get the most recent messages (messages after the command was sent).
     * 
     * @return Number of messages received in this poll
     */
    private suspend fun pollForResponses(): Int = withContext(Dispatchers.IO) {
        if (!isConnected()) {
            return@withContext 0
        }

        var messageCount = 0
        val messagesToDisplay = mutableListOf<MessageWithTimestamp>() // Collect messages to sort by receivedAt

        try {
            // Build URL with deviceId, topic filter (if tenantId is known), and start timestamp
            // The start parameter filters to get only messages received after the command timestamp
            val urlBuilder = StringBuilder("$API_BASE_URL/messages?deviceId=$deviceId")
            
            // Add topic filter if tenantId is known
            // This filters to only messages from this specific device and tenant
            if (tenantId != null && deviceId != null) {
                val topicFilter = "prod/$tenantId/m/d/$deviceId/d2c"
                urlBuilder.append("&topic=").append(URLEncoder.encode(topicFilter, "UTF-8"))
                android.util.Log.d("DirectShellClient", "Using topic filter: $topicFilter")
            }
            
            // Use timestamp for start parameter:
            // - If we have seen messages, use lastSeenReceivedAt + 1 second
            // - If not, use current time minus 30 seconds in UTC (Z time)
            val startTime = if (lastSeenReceivedAt != null) {
                // We have seen messages, use last seen receivedAt + 1 second
                addSecondsToIso8601Timestamp(lastSeenReceivedAt!!, 1)
            } else {
                // No messages seen yet, use current time minus 30 seconds in UTC
                val cal = Calendar.getInstance(TimeZone.getTimeZone("UTC"))
                cal.add(Calendar.SECOND, -30)
                getIso8601Timestamp(cal.time)
            }
            urlBuilder.append("&start=").append(URLEncoder.encode(startTime, "UTF-8"))
            android.util.Log.d("DirectShellClient", "Using start parameter: $startTime (lastSeenReceivedAt=${lastSeenReceivedAt}, filters to messages after this timestamp)")
            
            val url = urlBuilder.toString()
            
            android.util.Log.d("DirectShellClient", "Polling messages: $url")
            
            val request = Request.Builder()
                .url(url)
                .addHeader("Authorization", "Bearer $apiKey")
                .addHeader("Content-Type", "application/json")
                .addHeader("Connection", "close")
                .get()
                .build()

            // Make async call with timeout
            val response = async(Dispatchers.IO) {
                httpClient.newCall(request).execute()
            }.await()

            if (response.isSuccessful) {
                val responseBody = response.body?.string()
                if (responseBody != null && responseBody.isNotEmpty()) {
                    android.util.Log.d("DirectShellClient", "Received messages response: $responseBody")
                    
                    // Parse messages response
                    // API returns: {"items": [...], "total": N}
                    try {
                        val json = JsonParser.parseString(responseBody).asJsonObject
                        
                        // Check for "items" array (nRF Cloud messages API format)
                        // Format: {"items": [{"tenantId": "...", "topic": "...", "message": {"appId": "...", "data": "..."}}, ...], "total": N}
                        if (json.has("items") && json.get("items").isJsonArray) {
                            val items = json.get("items").asJsonArray
                            android.util.Log.d("DirectShellClient", "Found ${items.size()} items in response")
                            
                            for (itemElement in items) {
                                if (itemElement.isJsonObject) {
                                    val item = itemElement.asJsonObject
                                    
                                    // Extract and store tenantId from first message if not already stored
                                    if (tenantId == null && item.has("tenantId")) {
                                        tenantId = item.get("tenantId").asString
                                        android.util.Log.d("DirectShellClient", "Stored tenantId: $tenantId")
                                    }
                                    
                                    // Extract receivedAt timestamp to track duplicates
                                    val receivedAt = if (item.has("receivedAt")) item.get("receivedAt").asString else null
                                    
                                    // Update lastSeenReceivedAt if this is a new message
                                    if (receivedAt != null) {
                                        updateLastSeenReceivedAt(receivedAt)
                                    }
                                    
                                    // Check if we've already shown this message (by receivedAt or content hash)
                                    val messageKey = if (receivedAt != null) {
                                        receivedAt
                                    } else {
                                        null // Will check by content hash below
                                    }
                                    
                                    if (messageKey != null && shownMessageTimestamps.contains(messageKey)) {
                                        android.util.Log.d("DirectShellClient", "Skipping duplicate message with receivedAt: $receivedAt")
                                        continue // Skip this message, already shown
                                    }
                                    
                                    // Check if item has "message" field containing the actual message
                                    if (item.has("message") && item.get("message").isJsonObject) {
                                        val message = item.get("message").asJsonObject
                                        
                                        // Check topic - should end with /d/{deviceId}/d2c (device-to-cloud)
                                        // Format: prod/{tenant-id}/m/d/{device-id}/d2c or d/{device-id}/d2c
                                        val topic = if (item.has("topic")) item.get("topic").asString else ""
                                        
                                        // Verify topic matches device-to-cloud pattern for this device
                                        // Accept any topic ending with /d/{deviceId}/d2c (tenant ID prefix can vary)
                                        val expectedSuffix = "/d/$deviceId/d2c"
                                        val isD2cTopic = topic.endsWith(expectedSuffix)
                                        
                                        android.util.Log.d("DirectShellClient", "Item topic: $topic, matches d2c: $isD2cTopic")
                                        
                                        // Check for DECT_SHELL messages and verify it's a d2c message
                                        // Also check for DEVICE appId messages which contain shell output
                                        val appId = if (message.has("appId")) message.get("appId").asString else ""
                                        if (isD2cTopic && (appId == "DECT_SHELL" || appId == "DEVICE")) {
                                            if (message.has("data")) {
                                                val data = message.get("data").asString
                                                
                                                // Create content hash for duplicate detection (fallback if no receivedAt)
                                                val contentHash = if (data.length > 100) {
                                                    data.substring(0, 100).hashCode().toString()
                                                } else {
                                                    data.hashCode().toString()
                                                }
                                                
                                                // Check duplicate by content hash if no receivedAt
                                                val duplicateKey = if (receivedAt != null) {
                                                    receivedAt
                                                } else {
                                                    contentHash
                                                }
                                                
                                                // Check if already shown (by receivedAt or content hash)
                                                if (shownMessageTimestamps.contains(duplicateKey) || 
                                                    (receivedAt == null && shownMessageHashes.contains(contentHash))) {
                                                    android.util.Log.d("DirectShellClient", "Skipping duplicate message (receivedAt=$receivedAt, hash=$contentHash)")
                                                    continue
                                                }
                                                
                                                android.util.Log.d("DirectShellClient", "Found response (appId=$appId, receivedAt=$receivedAt): $data")
                                                // Collect message for sorting instead of displaying immediately
                                                messagesToDisplay.add(MessageWithTimestamp(data, receivedAt))
                                                // Mark this message as shown
                                                if (receivedAt != null) {
                                                    shownMessageTimestamps.add(receivedAt)
                                                } else {
                                                    shownMessageHashes.add(contentHash)
                                                }
                                                messageCount++
                                                // Don't return immediately - continue processing to collect all messages
                                            }
                                        }
                                    } else if (item.has("appId") && (item.get("appId").asString == "DECT_SHELL" || item.get("appId").asString == "DEVICE")) {
                                        // Message might be directly in item (fallback format)
                                        if (item.has("data")) {
                                            val data = item.get("data").asString
                                            
                                            // Create content hash for duplicate detection (fallback if no receivedAt)
                                            val contentHash = if (data.length > 100) {
                                                data.substring(0, 100).hashCode().toString()
                                            } else {
                                                data.hashCode().toString()
                                            }
                                            
                                            // Check duplicate by content hash if no receivedAt
                                            val duplicateKey = if (receivedAt != null) {
                                                receivedAt
                                            } else {
                                                contentHash
                                            }
                                            
                                            // Check if already shown (by receivedAt or content hash)
                                            if (shownMessageTimestamps.contains(duplicateKey) || 
                                                (receivedAt == null && shownMessageHashes.contains(contentHash))) {
                                                android.util.Log.d("DirectShellClient", "Skipping duplicate message (fallback format, receivedAt=$receivedAt, hash=$contentHash)")
                                                continue
                                            }
                                            
                                            android.util.Log.d("DirectShellClient", "Found response (fallback format, receivedAt=$receivedAt): $data")
                                            // Collect message for sorting instead of displaying immediately
                                            messagesToDisplay.add(MessageWithTimestamp(data, receivedAt))
                                            // Mark this message as shown
                                            if (receivedAt != null) {
                                                shownMessageTimestamps.add(receivedAt)
                                            } else {
                                                shownMessageHashes.add(contentHash)
                                            }
                                            messageCount++
                                        }
                                    }
                                }
                            }
                        } else if (json.has("messages") && json.get("messages").isJsonArray) {
                            // Alternative format with "messages" array
                            val messages = json.get("messages").asJsonArray
                            for (msgElement in messages) {
                                if (msgElement.isJsonObject) {
                                    val msg = msgElement.asJsonObject
                                    // Check receivedAt for duplicates (if available in this format)
                                    val receivedAt = if (msg.has("receivedAt")) msg.get("receivedAt").asString else null
                                    
                                    // Update lastSeenReceivedAt if this is a new message
                                    if (receivedAt != null) {
                                        updateLastSeenReceivedAt(receivedAt)
                                    }
                                    
                                    if (msg.has("appId") && msg.get("appId").asString == "DECT_SHELL") {
                                        if (msg.has("data")) {
                                            val data = msg.get("data").asString
                                            
                                            // Create content hash for duplicate detection (fallback if no receivedAt)
                                            val contentHash = if (data.length > 100) {
                                                data.substring(0, 100).hashCode().toString()
                                            } else {
                                                data.hashCode().toString()
                                            }
                                            
                                            // Check duplicate by content hash if no receivedAt
                                            val duplicateKey = if (receivedAt != null) {
                                                receivedAt
                                            } else {
                                                contentHash
                                            }
                                            
                                            // Check if already shown (by receivedAt or content hash)
                                            if (shownMessageTimestamps.contains(duplicateKey) || 
                                                (receivedAt == null && shownMessageHashes.contains(contentHash))) {
                                                android.util.Log.d("DirectShellClient", "Skipping duplicate message (messages array, receivedAt=$receivedAt, hash=$contentHash)")
                                                continue
                                            }
                                            
                                            android.util.Log.d("DirectShellClient", "Found DECT_SHELL response (receivedAt=$receivedAt): $data")
                                            // Collect message for sorting instead of displaying immediately
                                            messagesToDisplay.add(MessageWithTimestamp(data, receivedAt))
                                            if (receivedAt != null) {
                                                shownMessageTimestamps.add(receivedAt)
                                            } else {
                                                shownMessageHashes.add(contentHash)
                                            }
                                            messageCount++
                                        }
                                    }
                                }
                            }
                        } else if (json.has("appId") && json.get("appId").asString == "DECT_SHELL") {
                            // Single message object
                            val receivedAt = if (json.has("receivedAt")) json.get("receivedAt").asString else null
                            
                            // Update lastSeenReceivedAt if this is a new message
                            if (receivedAt != null) {
                                updateLastSeenReceivedAt(receivedAt)
                            }
                            
                            if (json.has("data")) {
                                val data = json.get("data").asString
                                
                                // Create content hash for duplicate detection (fallback if no receivedAt)
                                val contentHash = if (data.length > 100) {
                                    data.substring(0, 100).hashCode().toString()
                                } else {
                                    data.hashCode().toString()
                                }
                                
                                // Check duplicate by content hash if no receivedAt
                                val duplicateKey = if (receivedAt != null) {
                                    receivedAt
                                } else {
                                    contentHash
                                }
                                
                                // Check if already shown (by receivedAt or content hash)
                                if (shownMessageTimestamps.contains(duplicateKey) || 
                                    (receivedAt == null && shownMessageHashes.contains(contentHash))) {
                                    android.util.Log.d("DirectShellClient", "Skipping duplicate message (single object, receivedAt=$receivedAt, hash=$contentHash)")
                                } else {
                                    android.util.Log.d("DirectShellClient", "Found DECT_SHELL response (receivedAt=$receivedAt): $data")
                                    // Collect message for sorting instead of displaying immediately
                                    messagesToDisplay.add(MessageWithTimestamp(data, receivedAt))
                                    if (receivedAt != null) {
                                        shownMessageTimestamps.add(receivedAt)
                                    } else {
                                        shownMessageHashes.add(contentHash)
                                    }
                                    messageCount++
                                }
                            }
                        }
                        
                        android.util.Log.d("DirectShellClient", "No DECT_SHELL messages found in response")
                    } catch (e: Exception) {
                        android.util.Log.e("DirectShellClient", "Error parsing messages: ${e.message}", e)
                        android.util.Log.e("DirectShellClient", "Response body: $responseBody")
                    }
                } else {
                    android.util.Log.d("DirectShellClient", "No messages in response")
                }
            } else {
                android.util.Log.d("DirectShellClient", "Polling messages check: ${response.code} ${response.message}")
            }
            
            // Sort messages by receivedAt timestamp and display in order
            if (messagesToDisplay.isNotEmpty()) {
                val sortedMessages = messagesToDisplay.sortedWith(compareBy { msg ->
                    // Sort by receivedAt if available, otherwise put at end
                    if (msg.receivedAt != null) {
                        try {
                            val sdf = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US)
                            sdf.timeZone = TimeZone.getTimeZone("UTC")
                            sdf.parse(msg.receivedAt)?.time ?: Long.MAX_VALUE
                        } catch (e: Exception) {
                            Long.MAX_VALUE
                        }
                    } else {
                        Long.MAX_VALUE
                    }
                })
                
                // Display messages in sorted order
                for (msg in sortedMessages) {
                    onMessageReceived?.invoke(msg.data)
                }
            }
        } catch (e: TimeoutCancellationException) {
            android.util.Log.w("DirectShellClient", "Polling timeout: ${e.message}")
        } catch (e: Exception) {
            android.util.Log.e("DirectShellClient", "Error polling for responses: ${e.message}", e)
        }
        
        return@withContext messageCount
    }

    /**
     * Disconnect (stops polling)
     */
    fun disconnect() {
        android.util.Log.d("DirectShellClient", "Disconnect called")
        stopPolling()
        isConnected = false
        onConnectionStateChanged?.invoke(false)
    }

    /**
     * Get ISO 8601 formatted timestamp
     * Compatible with API 21+
     */
    private fun getIso8601Timestamp(date: Date = Date()): String {
        val sdf = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US)
        sdf.timeZone = TimeZone.getTimeZone("UTC")
        return sdf.format(date)
    }

    /**
     * Add seconds to an ISO 8601 timestamp string
     * @param iso8601Timestamp ISO 8601 formatted timestamp (e.g., "2025-11-17T06:40:12.830Z")
     * @param seconds Number of seconds to add (can be negative to subtract)
     * @return New ISO 8601 formatted timestamp
     */
    private fun addSecondsToIso8601Timestamp(iso8601Timestamp: String, seconds: Int): String {
        try {
            val sdf = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US)
            sdf.timeZone = TimeZone.getTimeZone("UTC")
            val date = sdf.parse(iso8601Timestamp)
            if (date != null) {
                val cal = Calendar.getInstance(TimeZone.getTimeZone("UTC"))
                cal.time = date
                cal.add(Calendar.SECOND, seconds)
                return getIso8601Timestamp(cal.time)
            }
        } catch (e: Exception) {
            android.util.Log.e("DirectShellClient", "Error parsing ISO8601 timestamp: $iso8601Timestamp", e)
        }
        // Fallback: return original timestamp if parsing fails
        return iso8601Timestamp
    }

    /**
     * Update lastSeenReceivedAt if the new receivedAt is later than the current one
     * Compares ISO8601 timestamps to find the latest
     */
    private fun updateLastSeenReceivedAt(receivedAt: String?) {
        if (receivedAt == null) return
        
        try {
            if (lastSeenReceivedAt == null) {
                // First message seen, just store it
                lastSeenReceivedAt = receivedAt
                android.util.Log.d("DirectShellClient", "Setting lastSeenReceivedAt: $receivedAt")
            } else {
                // Compare timestamps to find the latest
                // lastSeenReceivedAt is not null here (we're in else block)
                val sdf = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US)
                sdf.timeZone = TimeZone.getTimeZone("UTC")
                val currentDate = sdf.parse(lastSeenReceivedAt!!) // Safe: we're in else block where it's not null
                val newDate = sdf.parse(receivedAt)
                
                if (currentDate != null && newDate != null && newDate.after(currentDate)) {
                    // New timestamp is later, update it
                    lastSeenReceivedAt = receivedAt
                    android.util.Log.d("DirectShellClient", "Updated lastSeenReceivedAt to: $receivedAt")
                }
            }
        } catch (e: Exception) {
            android.util.Log.e("DirectShellClient", "Error comparing timestamps: $receivedAt vs $lastSeenReceivedAt", e)
        }
    }

    /**
     * Close and dispose resources
     */
    override fun close() {
        disconnect()
        pollingScope.cancel()
        deviceId = null
        apiKey = null
        tenantId = null
        lastCommandTimestamp = null
        lastSeenReceivedAt = null
        shownMessageTimestamps.clear()
        shownMessageHashes.clear()
    }
}

