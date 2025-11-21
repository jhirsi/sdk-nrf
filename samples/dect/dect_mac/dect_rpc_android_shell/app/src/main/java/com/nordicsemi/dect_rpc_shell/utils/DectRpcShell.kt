/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc_shell.utils

import com.nordicsemi.dect_rpc.DectRpcClient
import com.nordicsemi.dect_rpc_shell.services.CloudAuthService
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

/**
 * DECT RPC shell command executor
 */
class DectRpcShell {
    private var client: DectRpcClient? = null
    private var connected = false
    private val commandHistory = mutableListOf<String>()
    private var historyIndex = -1

    /**
     * Output event for displaying messages
     */
    var onOutputReceived: ((String) -> Unit)? = null

    /**
     * Connection state changed event
     */
    var onConnectionStateChanged: ((Boolean) -> Unit)? = null

    /**
     * Check if connected
     */
    fun isConnected(): Boolean = connected && client?.isConnected == true

    /**
     * Connect to nRF Cloud
     */
    suspend fun connect(deviceId: String, apiKey: String): Boolean {
        return try {
            if (!CloudAuthService.isValidDeviceId(deviceId)) {
                writeOutput("Error: Invalid device ID")
                return false
            }

            if (!CloudAuthService.isValidApiKey(apiKey)) {
                writeOutput("Error: Invalid API key")
                return false
            }

            writeOutput("Connecting to nRF Cloud (device: $deviceId)...")

            // Dispose old client if exists
            client?.close()

            // Create new client
            client = DectRpcClient()
            client!!.onConnectionStateChanged = { connected ->
                this.connected = connected
                onConnectionStateChanged?.invoke(connected)
                if (connected) {
                    writeOutput("Connected to nRF Cloud")
                } else {
                    writeOutput("Disconnected from nRF Cloud")
                }
            }

            client!!.onEventReceived = { event ->
                writeOutput("Event: ${event.eventId} on interface ${event.ifaceIndex}")
            }

            // Connect using REST API (no MQTT options needed)
            client!!.connect(deviceId, apiKey)

            connected = true
            writeOutput("Connection established")
            true
        } catch (ex: Exception) {
            writeOutput("Connection error: ${ex.message}")
            connected = false
            false
        }
    }

    /**
     * Execute a shell command
     */
    suspend fun executeCommand(command: String): Boolean {
        if (!isConnected()) {
            writeOutput("Not connected. Use 'connect <device_id> <api_key>' first.")
            return false
        }

        val parts = command.trim().split("\\s+".toRegex())
        if (parts.isEmpty()) {
            return false
        }

        val cmd = parts[0].lowercase()

        return when (cmd) {
            "help" -> {
                showHelp()
                true
            }
            "status" -> {
                try {
                    val status = client!!.getStatusInfo(0)
                    writeOutput("DECT NR+ status:")
                    if (status.fwVersionStr.isNotEmpty()) {
                        writeOutput("  Modem FW version:             ${status.fwVersionStr}")
                    }
                    writeOutput("  Modem activated:              ${if (status.modemActivated) "yes" else "no"}")
                    writeOutput("  Cluster running:              ${if (status.clusterRunning) "yes" else "no"}")
                    writeOutput("  Network beacon running:       ${if (status.nwBeaconRunning) "yes" else "no"}")
                    
                    // Print associations
                    if (status.parentCount > 0u || status.childCount > 0u) {
                        writeOutput("  Associations:")
                        
                        // Parent associations
                        for (i in 0 until status.parentAssociations.size) {
                            val assoc = status.parentAssociations[i]
                            writeOutput("    Parent long RD ID:              ${assoc.longRdId} (0x${assoc.longRdId.toString(16).uppercase()})")
                            val localAddr = assoc.localIpv6Addr
                            if (localAddr != null) {
                                writeOutput("      Local IPv6 address:           ${localAddr.hostAddress}")
                            }
                            val globalAddr = assoc.globalIpv6Addr
                            if (assoc.globalIpv6AddrSet && globalAddr != null) {
                                writeOutput("      Global IPv6 address:          ${globalAddr.hostAddress}")
                            }
                        }
                        
                        // Child associations
                        for (i in 0 until status.childAssociations.size) {
                            val assoc = status.childAssociations[i]
                            writeOutput("    Child long RD ID:               ${assoc.longRdId} (0x${assoc.longRdId.toString(16).uppercase()})")
                            val localAddr = assoc.localIpv6Addr
                            if (localAddr != null) {
                                writeOutput("      Local IPv6 address:           ${localAddr.hostAddress}")
                            }
                            val globalAddr = assoc.globalIpv6Addr
                            if (assoc.globalIpv6AddrSet && globalAddr != null) {
                                writeOutput("      Global IPv6 address:          ${globalAddr.hostAddress}")
                            }
                        }
                    }
                    true
                } catch (ex: Exception) {
                    writeOutput("Error getting status: ${ex.message}")
                    false
                }
            }
            "activate" -> {
                try {
                    val result = client!!.activate(0)
                    if (result == 0) {
                        writeOutput("DECT activated successfully")
                    } else {
                        writeOutput("DECT activation failed with error: $result")
                    }
                    true
                } catch (ex: Exception) {
                    writeOutput("Error activating DECT: ${ex.message}")
                    false
                }
            }
            "deactivate" -> {
                try {
                    val result = client!!.deactivate(0)
                    if (result == 0) {
                        writeOutput("DECT deactivated successfully")
                    } else {
                        writeOutput("DECT deactivation failed with error: $result")
                    }
                    true
                } catch (ex: Exception) {
                    writeOutput("Error deactivating DECT: ${ex.message}")
                    false
                }
            }
            "disconnect" -> {
                disconnect()
                true
            }
            else -> {
                writeOutput("Unknown command: $cmd. Type 'help' for available commands.")
                false
            }
        }
    }

    /**
     * Disconnect from nRF Cloud
     */
    fun disconnect() {
        client?.close()
        client = null
        connected = false
        onConnectionStateChanged?.invoke(false)
        writeOutput("Disconnected")
    }

    private fun showHelp() {
        writeOutput("Available commands:")
        writeOutput("  help          - Show this help message")
        writeOutput("  status        - Get DECT status information")
        writeOutput("  activate      - Activate DECT stack")
        writeOutput("  deactivate    - Deactivate DECT stack")
        writeOutput("  disconnect    - Disconnect from nRF Cloud")
    }

    private fun writeOutput(message: String) {
        onOutputReceived?.invoke(message)
    }
}

