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

/**
 * REST API client for sending direct DECT shell commands via nRF Cloud REST API
 * Commands are sent as JSON: {"appId":"DECT_SHELL", "data":"<command>"}
 * 
 * Uses ListMessages API endpoint to poll for responses after sending commands.
 * See: https://api.nrfcloud.com/v1/#tag/Messages/operation/ListMessages
 */
class DirectShellRestClient {
    private val API_BASE_URL = "https://api.nrfcloud.com/v1"
    
    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()
    
    private val gson = Gson()
    private var apiKey: String? = null
    private var deviceId: String? = null
    private var pollingJob: Job? = null
    private val pollingScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    
    /**
     * Polling configuration
     */
    var pollingTimeoutMs: Long = 20000 // Default 20 seconds (5 + 3*4 + buffer)
    var pollingIntervalMs: Long = 4000 // Default 4 seconds between polls
    var maxPollAttempts: Int = 4 // Default 4 attempts (1 initial + 3 more)
    var initialPollDelayMs: Long = 5000 // Wait 5 seconds before first poll

    /**
     * Event fired when a message is received from the device
     * Polling mechanism will call this when responses are found
     */
    var onMessageReceived: ((String) -> Unit)? = null

    /**
     * Event fired when polling times out (no response received)
     */
    var onPollingTimeout: (() -> Unit)? = null

    /**
     * Initialize the client with API key and device ID
     */
    fun initialize(deviceId: String, apiKey: String) {
        this.deviceId = deviceId
        this.apiKey = apiKey
    }

    /**
     * Send a DECT shell command via REST API
     * 
     * @param command The DECT shell command to send
     * @return true if command was sent successfully, false otherwise
     */
    suspend fun sendCommand(command: String): Boolean = withContext(Dispatchers.IO) {
        if (deviceId == null || apiKey == null) {
            android.util.Log.e("DirectShellRestClient", "Client not initialized")
            return@withContext false
        }

        try {
            // Create JSON message: {"appId":"DECT_SHELL", "data":"<command>"}
            val jsonMessage = JsonObject().apply {
                addProperty("appId", "DECT_SHELL")
                addProperty("data", command)
            }

            val jsonString = gson.toJson(jsonMessage)
            android.util.Log.d("DirectShellRestClient", "Sending command: $jsonString")

            // Create the message payload with topic
            val messagePayload = JsonObject().apply {
                add("message", jsonMessage)
                addProperty("topic", "$deviceId/c2d")
            }

            val requestBody = gson.toJson(messagePayload)
                .toRequestBody("application/json".toMediaType())

            // POST to /v1/devices/{deviceId}/messages
            val url = "$API_BASE_URL/devices/$deviceId/messages"
            val request = Request.Builder()
                .url(url)
                .addHeader("Authorization", "Bearer $apiKey")
                .addHeader("Content-Type", "application/json")
                .post(requestBody)
                .build()

            android.util.Log.d("DirectShellRestClient", "POST $url")
            val response = httpClient.newCall(request).execute()

            if (response.isSuccessful) {
                android.util.Log.d("DirectShellRestClient", "Command sent successfully")
                val responseBody = response.body?.string()
                android.util.Log.d("DirectShellRestClient", "Response: $responseBody")
                
                // Start limited polling for response in background (non-blocking)
                // Launch in separate coroutine to ensure sendCommand returns immediately
                pollingScope.launch {
                    startLimitedPolling()
                }
                
                return@withContext true
            } else {
                android.util.Log.e("DirectShellRestClient", "Failed to send command: ${response.code} ${response.message}")
                val errorBody = response.body?.string()
                android.util.Log.e("DirectShellRestClient", "Error response: $errorBody")
                return@withContext false
            }
        } catch (ex: TimeoutCancellationException) {
            android.util.Log.e("DirectShellRestClient", "Timeout sending command: ${ex.message}", ex)
            false
        } catch (ex: Exception) {
            android.util.Log.e("DirectShellRestClient", "Error sending command: ${ex.message}", ex)
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
            
            android.util.Log.d("DirectShellRestClient", "Starting limited polling: timeout=${pollingTimeoutMs}ms, interval=${pollingIntervalMs}ms, maxAttempts=$maxPollAttempts")
            
            try {
                // Wait before first poll
                delay(initialPollDelayMs)
                
                while (isActive && isInitialized()) {
                    val elapsed = System.currentTimeMillis() - startTime
                    
                    // Check timeout
                    if (elapsed >= pollingTimeoutMs) {
                        android.util.Log.d("DirectShellRestClient", "Polling timeout reached (${elapsed}ms)")
                        break
                    }
                    
                    // Check max attempts
                    if (attempt >= maxPollAttempts) {
                        android.util.Log.d("DirectShellRestClient", "Max polling attempts reached ($attempt)")
                        break
                    }
                    
                    attempt++
                    android.util.Log.d("DirectShellRestClient", "Polling attempt $attempt/$maxPollAttempts (elapsed: ${elapsed}ms)")
                    
                    // Poll for responses (non-blocking, uses suspend function)
                    pollForResponses()
                    
                    // Wait before next poll (unless we've reached timeout or max attempts)
                    if (attempt < maxPollAttempts && (System.currentTimeMillis() - startTime) < pollingTimeoutMs) {
                        delay(pollingIntervalMs)
                    }
                }
            } catch (e: CancellationException) {
                android.util.Log.d("DirectShellRestClient", "Polling cancelled")
                throw e // Re-throw cancellation
            } catch (e: Exception) {
                android.util.Log.e("DirectShellRestClient", "Polling error: ${e.message}", e)
            } finally {
                val elapsed = System.currentTimeMillis() - startTime
                android.util.Log.d("DirectShellRestClient", "Polling completed: $attempt attempts, ${elapsed}ms elapsed")
                
                // If we completed polling without finding a response, fire timeout event
                if (attempt >= maxPollAttempts || elapsed >= pollingTimeoutMs) {
                    android.util.Log.w("DirectShellRestClient", "Polling timeout - no response received")
                    onPollingTimeout?.invoke()
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
     * Uses start parameter to get only the most recent messages
     */
    private suspend fun pollForResponses() = withContext(Dispatchers.IO) {
        if (!isInitialized()) {
            return@withContext
        }

        try {
            // Poll messages endpoint for device-to-cloud (d2c) messages
            // GET /v1/messages?deviceId={deviceId}&start=0 to get only the most recent messages
            val url = "$API_BASE_URL/messages?deviceId=$deviceId&start=0"
            
            android.util.Log.d("DirectShellRestClient", "Polling messages: $url")
            
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
                    android.util.Log.d("DirectShellRestClient", "Received messages response: $responseBody")
                    
                    // Parse messages response
                    // API returns: {"items": [...], "total": N}
                    try {
                        val json = JsonParser.parseString(responseBody).asJsonObject
                        
                        // Check for "items" array (nRF Cloud messages API format)
                        // Format: {"items": [{"topic": "...", "message": {"appId": "...", "data": "..."}}, ...], "total": N}
                        if (json.has("items") && json.get("items").isJsonArray) {
                            val items = json.get("items").asJsonArray
                            android.util.Log.d("DirectShellRestClient", "Found ${items.size()} items in response")
                            
                            for (itemElement in items) {
                                if (itemElement.isJsonObject) {
                                    val item = itemElement.asJsonObject
                                    
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
                                        
                                        android.util.Log.d("DirectShellRestClient", "Item topic: $topic, matches d2c: $isD2cTopic")
                                        
                                        // Check for DECT_SHELL messages and verify it's a d2c message
                                        if (isD2cTopic && message.has("appId") && message.get("appId").asString == "DECT_SHELL") {
                                            if (message.has("data")) {
                                                val data = message.get("data").asString
                                                android.util.Log.d("DirectShellRestClient", "Found DECT_SHELL response: $data")
                                                onMessageReceived?.invoke(data)
                                                return@withContext
                                            }
                                        }
                                    } else if (item.has("appId") && item.get("appId").asString == "DECT_SHELL") {
                                        // Message might be directly in item (fallback format)
                                        if (item.has("data")) {
                                            val data = item.get("data").asString
                                            android.util.Log.d("DirectShellRestClient", "Found DECT_SHELL response: $data")
                                            onMessageReceived?.invoke(data)
                                            return@withContext
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
                                    if (msg.has("appId") && msg.get("appId").asString == "DECT_SHELL") {
                                        if (msg.has("data")) {
                                            val data = msg.get("data").asString
                                            android.util.Log.d("DirectShellRestClient", "Found DECT_SHELL response: $data")
                                            onMessageReceived?.invoke(data)
                                            return@withContext
                                        }
                                    }
                                }
                            }
                        } else if (json.has("appId") && json.get("appId").asString == "DECT_SHELL") {
                            // Single message object
                            if (json.has("data")) {
                                val data = json.get("data").asString
                                android.util.Log.d("DirectShellRestClient", "Found DECT_SHELL response: $data")
                                onMessageReceived?.invoke(data)
                                return@withContext
                            }
                        }
                        
                        android.util.Log.d("DirectShellRestClient", "No DECT_SHELL messages found in response")
                    } catch (e: Exception) {
                        android.util.Log.e("DirectShellRestClient", "Error parsing messages: ${e.message}", e)
                        android.util.Log.e("DirectShellRestClient", "Response body: $responseBody")
                    }
                } else {
                    android.util.Log.d("DirectShellRestClient", "No messages in response")
                }
            } else {
                android.util.Log.d("DirectShellRestClient", "Polling messages check: ${response.code} ${response.message}")
            }
        } catch (e: TimeoutCancellationException) {
            android.util.Log.w("DirectShellRestClient", "Polling timeout: ${e.message}")
        } catch (e: Exception) {
            android.util.Log.e("DirectShellRestClient", "Error polling for responses: ${e.message}", e)
        }
    }

    /**
     * Stop polling and cleanup
     */
    fun stop() {
        android.util.Log.d("DirectShellRestClient", "Stop called")
        stopPolling()
    }

    /**
     * Close and dispose resources
     */
    fun close() {
        stop()
        pollingScope.cancel()
        deviceId = null
        apiKey = null
    }

    /**
     * Check if client is initialized
     */
    fun isInitialized(): Boolean {
        return deviceId != null && apiKey != null
    }
}

