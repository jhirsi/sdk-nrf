/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc_shell.activities

import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.cardview.widget.CardView
import com.nordicsemi.dect_rpc_shell.models.CloudDevice
import com.nordicsemi.dect_rpc_shell.services.CloudRestService
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.Date

class DeviceListActivity : AppCompatActivity() {
    private lateinit var deviceRecyclerView: RecyclerView
    private lateinit var progressBar: ProgressBar
    private lateinit var statusText: TextView
    private lateinit var refreshButton: Button
    private var apiKey: String? = null
    private val devices = mutableListOf<CloudDevice>()
    private lateinit var adapter: DeviceAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        apiKey = intent.getStringExtra("ApiKey")
        if (apiKey.isNullOrEmpty()) {
            // Try to get from shared preferences
            val prefs = getSharedPreferences("DectRpcPrefs", MODE_PRIVATE)
            apiKey = prefs.getString("ApiKey", null)
        }

        if (apiKey.isNullOrEmpty()) {
            // No API key, go back to login
            startActivity(Intent(this, LoginActivity::class.java))
            finish()
            return
        }

        createUI()
        loadDevices()
    }

    private fun createUI() {
        // Main layout with proper system UI handling
        val mainLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            // Enable fitsSystemWindows to handle system bars properly
            fitsSystemWindows = true
        }

        // Toolbar with proper padding for system UI
        val toolbarLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setBackgroundColor(Color.parseColor("#2196F3"))
            // Add top padding to account for status bar
            setPadding(16, 48, 16, 16)
        }

        val titleText = TextView(this).apply {
            text = "nRF Cloud Devices"
            textSize = 20f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            setTextColor(Color.WHITE)
            layoutParams = LinearLayout.LayoutParams(
                0,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                1.0f
            )
        }
        toolbarLayout.addView(titleText)

        refreshButton = Button(this).apply {
            text = "Refresh"
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                marginEnd = 8
            }
            setOnClickListener { loadDevices() }
        }
        toolbarLayout.addView(refreshButton)

        // Logout button
        val logoutButton = Button(this).apply {
            text = "Logout"
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            setOnClickListener {
                // Clear saved API key
                val prefs = getSharedPreferences("DectRpcPrefs", MODE_PRIVATE)
                prefs.edit().remove("ApiKey").apply()

                // Navigate back to login
                startActivity(Intent(this@DeviceListActivity, LoginActivity::class.java))
                finish()
            }
        }
        toolbarLayout.addView(logoutButton)

        mainLayout.addView(toolbarLayout)

        // Status text
        statusText = TextView(this).apply {
            text = "Loading devices..."
            textSize = 14f
            gravity = android.view.Gravity.CENTER
            setPadding(16, 16, 16, 16)
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        mainLayout.addView(statusText)

        // Progress bar
        progressBar = ProgressBar(this).apply {
            isIndeterminate = true
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                topMargin = 16
                bottomMargin = 16
            }
        }
        mainLayout.addView(progressBar)

        // RecyclerView for devices (Grid layout like reference project)
        deviceRecyclerView = RecyclerView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                0,
                1.0f
            )
            // Use GridLayoutManager with 2 columns for card-like display
            layoutManager = GridLayoutManager(this@DeviceListActivity, 2)
            // Add padding for better spacing - extra bottom padding to prevent overlap
            setPadding(8, 8, 8, 16) // Reduced bottom padding since toolbar is now properly positioned
            clipToPadding = false
        }
        mainLayout.addView(deviceRecyclerView)

        setContentView(mainLayout)
    }

    private fun loadDevices() {
        progressBar.visibility = View.VISIBLE
        statusText.text = "Loading devices..."
        statusText.visibility = View.VISIBLE
        deviceRecyclerView.visibility = View.GONE
        refreshButton.isEnabled = false

        CoroutineScope(Dispatchers.Main).launch {
            try {
                val deviceList = withContext(Dispatchers.IO) {
                    CloudRestService.getDevices(apiKey!!)
                }

                devices.clear()
                devices.addAll(deviceList)

                if (devices.isEmpty()) {
                    statusText.text = "No devices found. Please add devices in nRF Cloud portal."
                    statusText.visibility = View.VISIBLE
                    deviceRecyclerView.visibility = View.GONE
                } else {
                    statusText.visibility = View.GONE
                    deviceRecyclerView.visibility = View.VISIBLE

                    adapter = DeviceAdapter(devices) { device ->
                        showShellSelectionDialog(device)
                    }
                    deviceRecyclerView.adapter = adapter
                }

                progressBar.visibility = View.GONE
                refreshButton.isEnabled = true
            } catch (ex: Exception) {
                statusText.text = "Error loading devices: ${ex.message}"
                statusText.visibility = View.VISIBLE
                deviceRecyclerView.visibility = View.GONE
                progressBar.visibility = View.GONE
                refreshButton.isEnabled = true
            }
        }
    }

    private fun showShellSelectionDialog(device: CloudDevice) {
        // Check MQTT connection status first
        CoroutineScope(Dispatchers.Main).launch {
            try {
                // Try to create a temporary MQTT client to check connection
                val mqttOptions = com.nordicsemi.dect_rpc_shell.services.CloudAuthService.createMqttConnectOptions(device.id, apiKey!!)
                val brokerUri = com.nordicsemi.dect_rpc_shell.services.CloudAuthService.getMqttBrokerUri()
                val testClient = org.eclipse.paho.client.mqttv3.MqttClient(brokerUri, device.id, org.eclipse.paho.client.mqttv3.persist.MemoryPersistence())
                
                var isConnected: Boolean
                try {
                    testClient.connect(mqttOptions)
                    isConnected = testClient.isConnected
                    testClient.disconnect()
                } catch (e: Exception) {
                    // Connection failed
                    isConnected = false
                } finally {
                    try {
                        if (testClient.isConnected) {
                            testClient.disconnect()
                        }
                    } catch (e: Exception) {
                        // Ignore disconnect errors
                    }
                }

                // Show dialog with connection status
                val message = if (isConnected) {
                    "MQTT connection: Connected\n\nChoose how to interact with the device:"
                } else {
                    "MQTT connection: Not connected\n\nYou can still try to connect, but it may fail.\n\nChoose how to interact with the device:"
                }

                AlertDialog.Builder(this@DeviceListActivity)
                    .setTitle("Select Shell Type for ${device.name}")
                    .setMessage(message)
                    .setPositiveButton("RPC Shell") { _, _ ->
                        // Navigate to RPC terminal
                        val intent = Intent(this@DeviceListActivity, TerminalActivity::class.java)
                        intent.putExtra("DeviceId", device.id)
                        intent.putExtra("ApiKey", apiKey)
                        intent.putExtra("DeviceName", device.name)
                        startActivity(intent)
                    }
                    .setNegativeButton("Direct Shell") { _, _ ->
                        // Navigate to direct shell
                        val intent = Intent(this@DeviceListActivity, DirectShellActivity::class.java)
                        intent.putExtra("DeviceId", device.id)
                        intent.putExtra("ApiKey", apiKey)
                        intent.putExtra("DeviceName", device.name)
                        intent.putExtra("TenantId", device.tenantId)
                        startActivity(intent)
                    }
                    .setNeutralButton("Cancel", null)
                    .show()
            } catch (ex: Exception) {
                // If check fails, show dialog anyway
                AlertDialog.Builder(this@DeviceListActivity)
                    .setTitle("Select Shell Type for ${device.name}")
                    .setMessage("Could not verify MQTT connection.\n\nChoose how to interact with the device:")
                    .setPositiveButton("RPC Shell") { _, _ ->
                        val intent = Intent(this@DeviceListActivity, TerminalActivity::class.java)
                        intent.putExtra("DeviceId", device.id)
                        intent.putExtra("ApiKey", apiKey)
                        intent.putExtra("DeviceName", device.name)
                        startActivity(intent)
                    }
                    .setNegativeButton("Direct Shell") { _, _ ->
                        val intent = Intent(this@DeviceListActivity, DirectShellActivity::class.java)
                        intent.putExtra("DeviceId", device.id)
                        intent.putExtra("ApiKey", apiKey)
                        intent.putExtra("DeviceName", device.name)
                        intent.putExtra("TenantId", device.tenantId)
                        startActivity(intent)
                    }
                    .setNeutralButton("Cancel", null)
                    .show()
            }
        }
    }
}

/**
 * RecyclerView adapter for device list
 */
class DeviceAdapter(
    private val devices: List<CloudDevice>,
    private val onDeviceClick: (CloudDevice) -> Unit
) : RecyclerView.Adapter<DeviceViewHolder>() {

    override fun getItemCount(): Int = devices.size

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DeviceViewHolder {
        // Create a card view for each device (like reference project)
        val cardView = CardView(parent.context).apply {
            layoutParams = ViewGroup.MarginLayoutParams(
                ViewGroup.MarginLayoutParams.MATCH_PARENT,
                ViewGroup.MarginLayoutParams.WRAP_CONTENT
            ).apply {
                val margin = 8
                setMargins(margin, margin, margin, margin)
            }
            radius = 12f
            cardElevation = 4f
            useCompatPadding = true
        }

        val contentLayout = LinearLayout(parent.context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(16, 16, 16, 16)
        }

        // Device icon (simple circle with device initial)
        val iconView = TextView(parent.context).apply {
            textSize = 36f
            gravity = android.view.Gravity.CENTER
            setPadding(0, 20, 0, 20)
            layoutParams = LinearLayout.LayoutParams(
                120, // Fixed width for consistent sizing
                120, // Fixed height for consistent sizing
                0.0f // Weight (Float) for center horizontal gravity
            ).apply {
                gravity = android.view.Gravity.CENTER_HORIZONTAL
            }
        }
        contentLayout.addView(iconView)

        // Device name
        val nameView = TextView(parent.context).apply {
            textSize = 16f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            gravity = android.view.Gravity.CENTER
            setPadding(0, 8, 0, 4)
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        contentLayout.addView(nameView)

        // Connection info line (Protocol/Method/Status)
        val connectionView = TextView(parent.context).apply {
            textSize = 10f
            gravity = android.view.Gravity.CENTER
            setPadding(0, 2, 0, 2)
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        contentLayout.addView(connectionView)
        
        // Board and firmware info line
        val boardFwView = TextView(parent.context).apply {
            textSize = 9f
            gravity = android.view.Gravity.CENTER
            setPadding(0, 2, 0, 2)
            setTextColor(Color.parseColor("#757575"))
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        contentLayout.addView(boardFwView)
        
        // Modem and hardware info line
        val modemHwView = TextView(parent.context).apply {
            textSize = 9f
            gravity = android.view.Gravity.CENTER
            setPadding(0, 2, 0, 2)
            setTextColor(Color.parseColor("#757575"))
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        contentLayout.addView(modemHwView)
        
        // Battery info line
        val batteryView = TextView(parent.context).apply {
            textSize = 9f
            gravity = android.view.Gravity.CENTER
            setPadding(0, 2, 0, 8)
            setTextColor(Color.parseColor("#757575"))
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        contentLayout.addView(batteryView)

        cardView.addView(contentLayout)

        return DeviceViewHolder(cardView, iconView, nameView, connectionView, boardFwView, modemHwView, batteryView, onDeviceClick)
    }

    override fun onBindViewHolder(holder: DeviceViewHolder, position: Int) {
        holder.bind(devices[position])
    }
}

/**
 * ViewHolder for device items (card layout like reference project)
 */
class DeviceViewHolder(
    itemView: View,
    private val iconView: TextView,
    private val deviceNameText: TextView,
    private val connectionView: TextView,
    private val boardFwView: TextView,
    private val modemHwView: TextView,
    private val batteryView: TextView,
    private val onDeviceClick: (CloudDevice) -> Unit
) : RecyclerView.ViewHolder(itemView) {
    private var device: CloudDevice? = null

    init {
        itemView.setOnClickListener {
            device?.let { onDeviceClick(it) }
        }
    }

    fun bind(device: CloudDevice) {
        this.device = device
        
        // Set device icon (first letter in a colored circle)
        val firstLetter = device.name.firstOrNull()?.uppercaseChar() ?: '?'
        iconView.text = firstLetter.toString()
        iconView.setTextColor(Color.WHITE)
        val bgColor = if (device.online) Color.parseColor("#4CAF50") else Color.parseColor("#9E9E9E")
        iconView.setBackgroundColor(bgColor)
        // Make it circular
        iconView.background = android.graphics.drawable.GradientDrawable().apply {
            shape = android.graphics.drawable.GradientDrawable.OVAL
            setColor(bgColor)
        }
        
        deviceNameText.text = device.name

        // Connection info line: Online/Offline • Protocol • Method • Status
        val connectionParts = mutableListOf<String>()
        if (device.online) {
            connectionParts.add("Online")
        } else {
            connectionParts.add("Offline")
        }
        
        device.protocol?.let { connectionParts.add(it) }
        device.method?.let { connectionParts.add(it) }
        device.connectionStatus?.let { connStatus ->
            if (connStatus != "connected" && connStatus != "disconnected") {
                connectionParts.add(connStatus)
            }
        }
        
        // Add time info
        device.lastSeen?.let { lastSeen ->
            val timeAgo = Date().time - lastSeen.time
            val minutes = timeAgo / (1000 * 60)
            val hours = timeAgo / (1000 * 60 * 60)
            val days = timeAgo / (1000 * 60 * 60 * 24)

            connectionParts.add(when {
                minutes < 1 -> "just now"
                hours < 1 -> "$minutes min ago"
                days < 1 -> "$hours hours ago"
                else -> "$days days ago"
            })
        }
        
        connectionView.text = connectionParts.joinToString(" • ")
        connectionView.setTextColor(
            if (device.online) Color.parseColor("#4CAF50") else Color.parseColor("#9E9E9E")
        )
        
        // Board and App Version line
        val boardFwParts = mutableListOf<String>()
        device.board?.let { boardFwParts.add(it) }
        device.firmware?.appVersion?.let { boardFwParts.add("v$it") }
        boardFwView.text = if (boardFwParts.isNotEmpty()) boardFwParts.joinToString(" • ") else ""
        
        // Modem and Hardware line
        val modemHwParts = mutableListOf<String>()
        device.firmware?.modemVersion?.let { modemHwParts.add("Modem:$it") }
        device.hardwareVersion?.let { modemHwParts.add("HW:$it") }
        modemHwView.text = if (modemHwParts.isNotEmpty()) modemHwParts.joinToString(" • ") else ""
        
        // Battery line
        device.batteryVoltage?.let {
            batteryView.text = String.format("%.2fV", it)
        } ?: run {
            batteryView.text = ""
        }
    }
}
