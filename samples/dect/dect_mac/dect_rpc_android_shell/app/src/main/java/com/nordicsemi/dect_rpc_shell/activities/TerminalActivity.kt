/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc_shell.activities

import android.graphics.Color
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
// R class will be generated automatically
import com.nordicsemi.dect_rpc_shell.utils.DectRpcShell
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class TerminalActivity : AppCompatActivity() {
    private lateinit var scrollView: ScrollView
    private lateinit var outputLayout: LinearLayout
    private lateinit var shell: DectRpcShell
    private var isProcessing = false
    private var deviceId: String? = null
    private var apiKey: String? = null
    private var deviceName: String? = null
    private var autoConnecting = false
    private val commandButtons = mutableListOf<Button>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Get device ID and API key from intent
        deviceId = intent.getStringExtra("DeviceId")
        apiKey = intent.getStringExtra("ApiKey")
        deviceName = intent.getStringExtra("DeviceName")

        // If no device ID, try to get from shared preferences (fallback)
        if (deviceId.isNullOrEmpty()) {
            val prefs = getSharedPreferences("DectRpcPrefs", MODE_PRIVATE)
            deviceId = prefs.getString("LastDeviceId", null)
            apiKey = prefs.getString("ApiKey", null)
        }

        // Create UI
        createUI()

        // Initialize shell
        shell = DectRpcShell()
        shell.onOutputReceived = { message -> writeOutput(message) }
        shell.onConnectionStateChanged = { connected ->
            runOnUiThread {
                if (connected) {
                    writeOutput("Connected to nRF Cloud")
                } else {
                    writeOutput("Disconnected from nRF Cloud")
                }
            }
        }

        // Show welcome message
        if (!deviceName.isNullOrEmpty()) {
            title = "DECT RPC Shell - $deviceName"
            writeOutput("DECT RPC Android Shell - $deviceName")
        } else {
            writeOutput("DECT RPC Android Shell")
        }
        writeOutput("")

        // Auto-connect if device ID and API key are available
        if (!deviceId.isNullOrEmpty() && !apiKey.isNullOrEmpty()) {
            autoConnect()
        } else {
            writeOutput("No device selected. Please select a device from the device list.")
        }
    }

    private fun autoConnect() {
        if (autoConnecting) {
            return
        }

        autoConnecting = true
        writeOutput("Auto-connecting to device: $deviceId...")

        CoroutineScope(Dispatchers.Main).launch {
            val connected = shell.connect(deviceId!!, apiKey!!)
            autoConnecting = false
            if (!connected) {
                writeOutput("Auto-connect failed. Please check your connection settings.")
            }
        }
    }

    private fun createUI() {
        val mainLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.WHITE)
            setPadding(16, 16, 16, 16)
        }

        // Output area (plain text)
        scrollView = ScrollView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                0,
                1.0f
            )
            setBackgroundColor(Color.WHITE)
        }

        outputLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(16, 16, 16, 16)
            setBackgroundColor(Color.WHITE)
        }
        scrollView.addView(outputLayout)
        mainLayout.addView(scrollView)

        // Buttons area
        val buttonsLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, 16, 0, 0)
            setBackgroundColor(Color.WHITE)
        }

        // Create buttons for each command
        val commands = listOf(
            "Help" to "help",
            "Status" to "status",
            "Activate" to "activate",
            "Deactivate" to "deactivate",
            "Disconnect" to "disconnect"
        )

        commands.forEach { (buttonText, command) ->
            val button = Button(this).apply {
                text = buttonText
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    bottomMargin = 8
                }
                setOnClickListener {
                    executeCommand(command)
                }
            }
            commandButtons.add(button)
            buttonsLayout.addView(button)
        }

        mainLayout.addView(buttonsLayout)
        setContentView(mainLayout)
    }

    private fun executeCommand(command: String) {
        if (isProcessing) {
            writeOutput("Please wait for the previous command to complete.")
            return
        }

        // Show which command was executed
        writeOutput("Executing: $command")
        writeOutput("")

        // Process command
        isProcessing = true
        updateButtonsEnabled(false)

        CoroutineScope(Dispatchers.Main).launch {
            try {
                shell.executeCommand(command)
            } catch (e: Exception) {
                android.util.Log.e("TerminalActivity", "Error executing command: ${e.message}", e)
                writeOutput("Error: ${e.message}")
            } finally {
                // Always re-enable buttons, even on error
                isProcessing = false
                updateButtonsEnabled(true)
            }
        }
    }

    private fun updateButtonsEnabled(enabled: Boolean) {
        runOnUiThread {
            commandButtons.forEach { button ->
                button.isEnabled = enabled
            }
        }
    }

    private fun writeOutput(message: String) {
        runOnUiThread {
            val textView = TextView(this).apply {
                text = message
                textSize = 16f
                setTextColor(Color.BLACK) // Plain black text
                setPadding(0, 4, 0, 4)
                setBackgroundColor(Color.WHITE)
            }
            outputLayout.addView(textView)
            scrollView.post {
                scrollView.fullScroll(View.FOCUS_DOWN)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        shell.disconnect()
    }
}
