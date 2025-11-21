/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc_shell.activities

import android.graphics.Color
import android.graphics.Typeface
import android.os.Bundle
import android.view.View
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
// R class will be generated automatically
import com.nordicsemi.dect_rpc_shell.services.CloudAuthService
import com.nordicsemi.dect_rpc_shell.services.DirectShellClient
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class DirectShellActivity : AppCompatActivity() {
    private lateinit var scrollView: ScrollView
    private lateinit var outputLayout: LinearLayout
    private lateinit var commandInput: EditText
    private var client: DirectShellClient? = null
    private var isProcessing = false
    private var deviceId: String? = null
    private var apiKey: String? = null
    private var deviceName: String? = null
    private var tenantId: String? = null
    private var autoConnecting = false
    private var lastEchoedLength = 0
    private var lastSentCommand: String? = null // Track last sent command to filter echo

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Get device ID and API key from intent
        deviceId = intent.getStringExtra("DeviceId")
        apiKey = intent.getStringExtra("ApiKey")
        deviceName = intent.getStringExtra("DeviceName")
        tenantId = intent.getStringExtra("TenantId")

        // Create UI
        createUI()

        // Initialize client
        client = DirectShellClient()
        client?.onMessageReceived = { message -> onMessageReceived(message) }
        client?.onConnectionStateChanged = { connected ->
            runOnUiThread {
                if (connected) {
                    // Add "Connected to nRF Cloud" at the very top (uppermost)
                    addOutputAtTop("Connected to nRF Cloud")
                } else {
                    writeOutput("Disconnected from nRF Cloud")
                }
            }
        }
        client?.onPollingTimeout = {
            runOnUiThread {
                hidePollingIndicator()
                writeError("Timeout: No response received from device")
                // Clear the command from input field after timeout
                commandInput.setText("")
                lastEchoedLength = 0
                lastSentCommand = null // Clear stored command after timeout
                // Re-enable input after timeout
                isProcessing = false
                commandInput.isEnabled = true
                commandInput.requestFocus()
                // Show keyboard when input is re-enabled
                val imm = getSystemService(android.content.Context.INPUT_METHOD_SERVICE) as android.view.inputmethod.InputMethodManager
                imm.showSoftInput(commandInput, android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT)
                // Ensure prompt is at bottom and scroll to it
                if (lastPromptTextView == null) {
                    showNewPrompt()
                } else {
                    scrollView.postDelayed({
                        scrollView.fullScroll(View.FOCUS_DOWN)
                    }, 100)
                }
            }
        }
        client?.onPollingComplete = {
            runOnUiThread {
                hidePollingIndicator()
                // Show message that polling stopped
                writeOutput("Polling for command response stopped.")
                // Polling completed (empty responses detected) - all data received
                // Clear the command from input field now that we have the response
                commandInput.setText("")
                lastEchoedLength = 0
                lastSentCommand = null // Clear stored command after polling completes
                // Re-enable input
                isProcessing = false
                commandInput.isEnabled = true
                commandInput.requestFocus()
                // Show keyboard when input is re-enabled
                val imm = getSystemService(android.content.Context.INPUT_METHOD_SERVICE) as android.view.inputmethod.InputMethodManager
                imm.showSoftInput(commandInput, android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT)
                // Ensure prompt is at bottom and scroll to it
                if (lastPromptTextView == null) {
                    showNewPrompt()
                } else {
                    // Scroll to bottom after a short delay to ensure keyboard is handled
                    scrollView.postDelayed({
                        scrollView.fullScroll(View.FOCUS_DOWN)
                    }, 200)
                }
            }
        }

        // Show welcome message with shell prompt (terminal style)
        if (!deviceName.isNullOrEmpty()) {
            title = "DECT Shell - $deviceName"
        } else {
            title = "DECT Shell"
        }
        
        writeOutput("Welcome to DECT Shell")
        writeOutput("Type 'help' for available commands")
        writeOutput("")
        
        // Show prompt at bottom (only one prompt)
        showNewPrompt()
        
        // Focus and keyboard will be handled in createUI() after layout is complete

        // Auto-connect if device ID and API key are available
        if (!deviceId.isNullOrEmpty() && !apiKey.isNullOrEmpty()) {
            autoConnect()
        } else {
            writeOutput("No device selected. Use 'connect <device_id> <api_key>' to connect.")
        }
    }

    private fun autoConnect() {
        if (autoConnecting) {
            android.util.Log.d("DirectShellActivity", "Auto-connect already in progress")
            return
        }

        autoConnecting = true
        android.util.Log.d("DirectShellActivity", "Starting auto-connect...")
        android.util.Log.d("DirectShellActivity", "Device ID: $deviceId")
        android.util.Log.d("DirectShellActivity", "API Key: ${if (apiKey.isNullOrEmpty()) "null/empty" else "provided (${apiKey!!.length} chars)"}")
        
        // Connect silently in the background (no message shown to user)
        CoroutineScope(Dispatchers.Main).launch {
            try {
                android.util.Log.d("DirectShellActivity", "Calling client.connect()...")
                android.util.Log.d("DirectShellActivity", "TenantId: $tenantId")
                val connected = client?.connect(deviceId!!, apiKey!!, tenantId) ?: false
                android.util.Log.d("DirectShellActivity", "Connection result: $connected")
                autoConnecting = false
                if (!connected) {
                    // Only show message if connection fails
                    android.util.Log.e("DirectShellActivity", "Auto-connect failed")
                    writeOutput("Connection failed. Use 'connect <device_id> <api_key>' to retry.")
                    writeOutput("Check logcat with: adb logcat -s DirectShellClient:D DirectShellActivity:D")
                } else {
                    android.util.Log.d("DirectShellActivity", "Auto-connect successful")
                }
                // If successful, the connection state callback will show "Connected to nRF Cloud"
            } catch (e: Exception) {
                android.util.Log.e("DirectShellActivity", "Exception during auto-connect", e)
                autoConnecting = false
                writeOutput("Connection error: ${e.message}")
                writeOutput("Check logcat for details")
            }
        }
    }

    // Extension function to convert dp to pixels
    private fun Int.dpToPx(): Int {
        val density = resources.displayMetrics.density
        return (this * density).toInt()
    }
    
    private fun createUI() {
        val mainLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.BLACK) // Terminal black background
            // Ensure layout fills screen properly
            fitsSystemWindows = false
        }

        // Output area (terminal-like)
        scrollView = ScrollView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                0,
                1.0f
            )
            setBackgroundColor(Color.BLACK) // Terminal black background
            // Enable scrollbar and make it always visible
            isScrollbarFadingEnabled = false // Keep scrollbar always visible
            scrollBarStyle = View.SCROLLBARS_INSIDE_INSET // Show scrollbar inside with padding (most visible)
            // Force scrollbar to be visible
            isVerticalScrollBarEnabled = true
            isHorizontalScrollBarEnabled = false
            // Set scrollbar size for better visibility (12dp - larger for better visibility)
            val scrollBarSizePx = 12.dpToPx()
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
                scrollBarSize = scrollBarSizePx
            }
            // Ensure scroll view can scroll properly
            isFillViewport = false
            // Note: scrollBarThumbVertical and scrollBarTrackVertical are not available on ScrollView
            // Scrollbar colors are controlled by theme/system
        }

        outputLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(16, 16, 16, 16)
            setBackgroundColor(Color.BLACK) // Terminal black background
        }
        scrollView.addView(outputLayout)
        mainLayout.addView(scrollView)

        // Input area (terminal-like with black background)
        // Add bottom padding to account for navigation bar
        val inputLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(8, 8, 8, 8)
            setBackgroundColor(Color.BLACK) // Terminal black background
            // Add extra bottom padding for navigation bar (will be adjusted by insets if available)
            setPaddingRelative(8, 8, 8, 24)
        }

        // Create prompt label (green terminal prompt)
        val promptLabel = TextView(this).apply {
            text = "desh:~$ "
            textSize = 14f
            typeface = Typeface.MONOSPACE
            setTextColor(Color.parseColor("#4CAF50")) // Green prompt
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        inputLayout.addView(promptLabel)

        commandInput = EditText(this).apply {
            hint = ""
            layoutParams = LinearLayout.LayoutParams(
                0,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                1.0f
            ).apply {
                marginStart = 4
                marginEnd = 8
            }
            typeface = Typeface.MONOSPACE
            setTextColor(Color.WHITE) // White text on black - ensures input is visible
            setHintTextColor(Color.parseColor("#888888")) // Gray hint
            setBackgroundColor(Color.BLACK) // Black background
            // Remove default EditText underline
            background = null
            setPadding(8, 8, 8, 8)
            // Ensure text input is enabled and visible
            isEnabled = true
            isFocusable = true
            isFocusableInTouchMode = true
            // Ensure text is visible as user types (default behavior, but explicit)
            inputType = android.text.InputType.TYPE_CLASS_TEXT or android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
            // Make sure text is always visible (no hiding)
            textSize = 14f
            // Ensure cursor is visible
            setTextIsSelectable(false) // Don't allow selection to keep it simple
            // Note: Cursor color is automatically white on black background with setTextColor(Color.WHITE)
            setOnEditorActionListener { _, _, _ ->
                sendCommand()
                true
            }
            // Add text watcher to echo input to output area as user types
            addTextChangedListener(object : android.text.TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                    // Echo input to output area as user types (terminal-style)
                    val currentText = s?.toString() ?: ""
                    if (currentText.length > lastEchoedLength) {
                        // New characters typed - echo them
                        // Only echo if not processing a command
                        if (!isProcessing) {
                            // Update the last prompt line with current input
                            updateLastPromptLine(currentText)
                        }
                    } else if (currentText.length < lastEchoedLength) {
                        // Characters deleted - update echo
                        if (!isProcessing) {
                            updateLastPromptLine(currentText)
                        }
                    }
                    lastEchoedLength = currentText.length
                    // Ensure cursor stays at end
                    setSelection(currentText.length)
                }
                override fun afterTextChanged(s: android.text.Editable?) {}
            })
        }
        inputLayout.addView(commandInput)

        // Remove send button - terminal doesn't have a send button, just Enter key
        // sendButton = Button(this).apply {
        //     text = "Send"
        //     setOnClickListener { sendCommand() }
        // }
        // inputLayout.addView(sendButton)

        mainLayout.addView(inputLayout)
        setContentView(mainLayout)
        
        // Apply window insets to respect system bars (navigation bar, status bar)
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            mainLayout.setOnApplyWindowInsetsListener { view, insets ->
                val systemBars = if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
                    view.rootWindowInsets.getInsets(android.view.WindowInsets.Type.systemBars())
                } else {
                    @Suppress("DEPRECATION")
                    android.graphics.Insets.of(
                        insets.systemWindowInsetLeft,
                        insets.systemWindowInsetTop,
                        insets.systemWindowInsetRight,
                        insets.systemWindowInsetBottom
                    )
                }
                
                // Add padding to input layout to account for navigation bar
                inputLayout.setPaddingRelative(
                    inputLayout.paddingStart,
                    inputLayout.paddingTop,
                    inputLayout.paddingEnd,
                    systemBars.bottom + 8
                )
                
                // Add padding to scroll view to account for status bar only (bottom padding handled by keyboard listener)
                scrollView.setPadding(
                    scrollView.paddingLeft,
                    systemBars.top,
                    scrollView.paddingRight,
                    scrollView.paddingBottom // Keep existing bottom padding (may be set by keyboard listener)
                )
                
                insets
            }
        }
        
        // Ensure keyboard shows when EditText gets focus
        commandInput.setOnFocusChangeListener { view, hasFocus ->
            if (hasFocus) {
                // Show keyboard when EditText gets focus
                val imm = getSystemService(android.content.Context.INPUT_METHOD_SERVICE) as android.view.inputmethod.InputMethodManager
                imm.showSoftInput(view, android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT)
            }
        }
        
        // Listen for keyboard visibility changes to ensure proper scrolling and input visibility
        var lastHeightDiff = 0
        val originalOutputLayoutBottomPadding = 16
        mainLayout.viewTreeObserver.addOnGlobalLayoutListener {
            val rootHeight = mainLayout.rootView.height
            val layoutHeight = mainLayout.height
            val heightDiff = rootHeight - layoutHeight
            
            // If height difference is significant, keyboard is likely visible
            if (heightDiff > 200 && heightDiff != lastHeightDiff) {
                lastHeightDiff = heightDiff
                // Keyboard is visible - add padding to outputLayout to prevent content going under keyboard
                val keyboardHeight = heightDiff
                // Get the input layout height to account for it
                val inputLayoutHeight = inputLayout.height
                // Calculate total space needed: keyboard height + input layout height + extra padding
                // This ensures content doesn't go under the keyboard or input area
                val newBottomPadding = keyboardHeight + inputLayoutHeight + 48 // Extra padding for safety
                
                // Set bottom padding on outputLayout (inside scroll view) to prevent content going under keyboard
                // This ensures the content itself has padding, not just the scroll view
                outputLayout.setPadding(
                    outputLayout.paddingLeft,
                    outputLayout.paddingTop,
                    outputLayout.paddingRight,
                    newBottomPadding
                )
                // Force layout update immediately
                outputLayout.requestLayout()
                scrollView.requestLayout()
                // Scroll to bottom to show latest content after layout
                scrollView.post {
                    scrollView.fullScroll(View.FOCUS_DOWN)
                    scrollView.postDelayed({
                        scrollView.fullScroll(View.FOCUS_DOWN)
                    }, 100)
                }
            } else if (heightDiff <= 200 && lastHeightDiff > 200) {
                // Keyboard was hidden - restore padding
                lastHeightDiff = heightDiff
                outputLayout.setPadding(
                    outputLayout.paddingLeft,
                    outputLayout.paddingTop,
                    outputLayout.paddingRight,
                    originalOutputLayoutBottomPadding // Restore original bottom padding
                )
                // Force layout update
                outputLayout.requestLayout()
                scrollView.requestLayout()
                scrollView.post {
                    scrollView.fullScroll(View.FOCUS_DOWN)
                }
            }
        }
        
        // Request focus and show keyboard after layout is complete
        commandInput.postDelayed({
            commandInput.requestFocus()
            val imm = getSystemService(android.content.Context.INPUT_METHOD_SERVICE) as android.view.inputmethod.InputMethodManager
            imm.showSoftInput(commandInput, android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT)
        }, 300)
    }

    private fun sendCommand() {
        if (isProcessing) {
            return
        }

        val command = commandInput.text.toString().trim()
        if (command.isEmpty()) {
            // Empty command - just show new prompt (go to next shell row)
            commandInput.setText("")
            lastEchoedLength = 0
            showNewPrompt()
            return
        }

        // Handle special commands
        if (command.lowercase() == "help") {
            showHelp()
            commandInput.setText("")
            showNewPrompt()
            return
        }

        if (command.startsWith("connect ")) {
            val parts = command.substring(8).split("\\s+".toRegex(), limit = 2)
            if (parts.size == 2) {
                connect(parts[0], parts[1])
            } else {
                writeOutput("Usage: connect <device_id> <api_key>")
                showNewPrompt()
            }
            commandInput.setText("")
            return
        }

        if (command.lowercase() == "disconnect") {
            disconnect()
            commandInput.setText("")
            showNewPrompt()
            return
        }

        // Send command
        if (client?.isConnected() != true) {
            writeOutput("Not connected. Use 'connect <device_id> <api_key>' first.")
            commandInput.setText("")
            return
        }

        // Show command in output with prompt FIRST (before clearing input)
        // Update the last prompt line with the command (this shows what was typed)
        updateLastPromptLine(command)
        
        // Add newline after command (CR+LF) but keep command visible in input field
        // Don't clear input - keep it visible while waiting for response
        // Just move to next line for the prompt
        
        // Show empty prompt for next command (after the entered command)
        showNewPrompt()
        
        // Keep the command in input field visible (don't clear it)
        // It will be cleared when we get a response or timeout
        // lastEchoedLength stays as is - we want to keep showing the command

        // Store command to filter echo from remote
        lastSentCommand = command
        
        // Send command asynchronously (don't block UI)
        isProcessing = true
        // Show polling indicator
        showPollingIndicator()

        CoroutineScope(Dispatchers.Main).launch {
            try {
                // Send command (this should return immediately after starting polling)
                val success = client?.sendCommand(command) ?: false
                if (!success) {
                    hidePollingIndicator()
                    writeError("Failed to send command")
                    // Re-enable input on failure
                    isProcessing = false
                    commandInput.isEnabled = true
                    commandInput.requestFocus()
                    showNewPrompt()
                }
                // On success, polling indicator will be hidden when response is received or timeout occurs
                // (handled in onPollingComplete and onPollingTimeout callbacks)
            } catch (e: Exception) {
                hidePollingIndicator()
                android.util.Log.e("DirectShellActivity", "Error sending command: ${e.message}", e)
                writeError("Error: ${e.message}")
                // Re-enable input on error
                isProcessing = false
                commandInput.isEnabled = true
                commandInput.requestFocus()
                showNewPrompt()
            }
        }
    }

    private fun connect(deviceId: String, apiKey: String) {
        if (!CloudAuthService.isValidDeviceId(deviceId)) {
            writeOutput("Error: Invalid device ID")
            showNewPrompt()
            return
        }

        if (!CloudAuthService.isValidApiKey(apiKey)) {
            writeOutput("Error: Invalid API key")
            showNewPrompt()
            return
        }

        writeOutput("Connecting to nRF Cloud (device: $deviceId)...")

        CoroutineScope(Dispatchers.Main).launch {
            val connected = client?.connect(deviceId, apiKey, null) ?: false
            if (connected) {
                this@DirectShellActivity.deviceId = deviceId
                this@DirectShellActivity.apiKey = apiKey
                writeOutput("Connection established")
            } else {
                writeOutput("Connection failed")
            }
            showNewPrompt()
        }
    }

    private fun disconnect() {
        client?.disconnect()
        writeOutput("Disconnected")
        showNewPrompt()
    }

    private fun showHelp() {
        writeOutput("Available commands:")
        writeOutput("  help                    - Show this help message")
        writeOutput("  connect <id> <key>      - Connect to device")
        writeOutput("  disconnect              - Disconnect from device")
        writeOutput("  <dect_command>          - Send DECT shell command")
        writeOutput("")
        writeOutput("Examples:")
        writeOutput("  dect status")
        writeOutput("  dect activate")
        showNewPrompt()
    }

    private fun onMessageReceived(message: String) {
        runOnUiThread {
            // Parse JSON response and extract the actual message, hiding all JSON formatting
            val cleanMessage = extractCleanMessage(message)
            // Only display if there's actual content (skip empty responses)
            if (cleanMessage.isNotEmpty()) {
                // Filter out "nRF Cloud event:" prefixed messages
                val trimmedMessage = cleanMessage.trim()
                if (trimmedMessage.startsWith("nRF Cloud event:", ignoreCase = true)) {
                    android.util.Log.d("DirectShellActivity", "Filtered out nRF Cloud event: $trimmedMessage")
                    return@runOnUiThread
                }
                
                // Filter out command echo from remote - don't reprint the command
                val trimmedCommand = lastSentCommand?.trim()
                
                // Check if the message is just the echoed command (exact match or with prompt)
                val isCommandEcho = if (trimmedCommand != null) {
                    trimmedMessage == trimmedCommand || 
                    trimmedMessage.endsWith(trimmedCommand) ||
                    trimmedMessage.contains(trimmedCommand) && trimmedMessage.length <= trimmedCommand.length + 10
                } else {
                    false
                }
                
                if (!isCommandEcho) {
                    // Display response - writeOutput() will handle moving prompt below responses
                    // Preserve all formatting including indentation and newlines
                    writeOutput(cleanMessage)
                } else {
                    android.util.Log.d("DirectShellActivity", "Filtered out command echo: $trimmedMessage")
                }
            }
            // Don't re-enable input or show prompt here - wait for onPollingComplete
            // This ensures we wait for all messages before showing the prompt
        }
    }
    
    private var lastPromptTextView: TextView? = null
    private var pollingIndicatorTextView: TextView? = null
    
    /**
     * Add output at the top of the output area (for connection status, etc.)
     */
    private fun addOutputAtTop(message: String) {
        runOnUiThread {
            val textView = TextView(this).apply {
                text = message
                textSize = 14f
                typeface = Typeface.MONOSPACE
                setTextColor(Color.WHITE)
                setPadding(0, 2, 0, 2)
                setBackgroundColor(Color.BLACK)
            }
            // Insert at position 0 (top)
            outputLayout.addView(textView, 0)
            
            // Auto-scroll to bottom to show prompt
            scrollView.post {
                scrollView.fullScroll(View.FOCUS_DOWN)
            }
        }
    }
    
    /**
     * Show a new command prompt (visual indicator that input is ready)
     * Writes the prompt to the output area so it's always visible at the bottom
     */
    private fun showNewPrompt() {
        // Write the prompt to output area (without command text)
        // Always add at the end to keep it at the bottom
        runOnUiThread {
            val textView = TextView(this).apply {
                textSize = 14f
                typeface = Typeface.MONOSPACE
                setPadding(0, 2, 0, 2)
                setBackgroundColor(Color.BLACK)
                
                // Create SpannableString to color prompt green
                val fullText = "desh:~$ "
                val spannable = android.text.SpannableString(fullText)
                spannable.setSpan(
                    android.text.style.ForegroundColorSpan(Color.parseColor("#4CAF50")),
                    0,
                    fullText.length,
                    android.text.Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
                )
                text = spannable
            }
            // Always add at the end to keep prompt at bottom
            outputLayout.addView(textView)
            lastPromptTextView = textView
            lastEchoedLength = 0 // Reset echo length for new prompt
            
            // Auto-scroll to show the new prompt at bottom
            // Use postDelayed to ensure keyboard is handled
            scrollView.postDelayed({
                scrollView.fullScroll(View.FOCUS_DOWN)
                // Also try smooth scroll as fallback
                val scrollAmount = scrollView.getChildAt(0)?.height ?: 0
                if (scrollAmount > 0) {
                    scrollView.smoothScrollTo(0, scrollAmount)
                }
            }, 100)
        }
        // Ensure input is focused
        commandInput.post {
            commandInput.requestFocus()
        }
    }
    
    /**
     * Show polling indicator (spinner/text) to show progress
     */
    private fun showPollingIndicator() {
        runOnUiThread {
            // Remove existing indicator if any
            pollingIndicatorTextView?.let {
                outputLayout.removeView(it)
            }
            
            val textView = TextView(this).apply {
                text = "⏳ Polling for response..."
                textSize = 12f
                typeface = Typeface.MONOSPACE
                setTextColor(Color.parseColor("#888888")) // Gray color
                setPadding(0, 2, 0, 2)
                setBackgroundColor(Color.BLACK)
            }
            // Add before the last prompt (so it's above the prompt)
            val insertIndex = if (lastPromptTextView != null) {
                val promptIndex = outputLayout.indexOfChild(lastPromptTextView)
                if (promptIndex >= 0) promptIndex else outputLayout.childCount
            } else {
                outputLayout.childCount
            }
            outputLayout.addView(textView, insertIndex)
            pollingIndicatorTextView = textView
            
            // Auto-scroll to show the indicator
            scrollView.post {
                scrollView.fullScroll(View.FOCUS_DOWN)
            }
        }
    }
    
    /**
     * Hide polling indicator
     */
    private fun hidePollingIndicator() {
        runOnUiThread {
            pollingIndicatorTextView?.let {
                outputLayout.removeView(it)
                pollingIndicatorTextView = null
            }
        }
    }
    
    /**
     * Update the last prompt line with current input (for echoing)
     */
    private fun updateLastPromptLine(inputText: String) {
        runOnUiThread {
            lastPromptTextView?.let { textView ->
                val fullText = "desh:~$ $inputText"
                val spannable = android.text.SpannableString(fullText)
                
                // Color the prompt part green
                val promptEnd = "desh:~$ ".length
                spannable.setSpan(
                    android.text.style.ForegroundColorSpan(Color.parseColor("#4CAF50")),
                    0,
                    promptEnd,
                    android.text.Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
                )
                
                // Color the input part white
                if (inputText.isNotEmpty()) {
                    spannable.setSpan(
                        android.text.style.ForegroundColorSpan(Color.WHITE),
                        promptEnd,
                        fullText.length,
                        android.text.Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
                    )
                }
                
                textView.text = spannable
                
                // Auto-scroll to show the updated prompt
                scrollView.postDelayed({
                    scrollView.fullScroll(View.FOCUS_DOWN)
                }, 50)
            }
        }
    }
    
    /**
     * Extract clean message from JSON response, hiding all JSON formatting from user.
     * Handles various response formats:
     * - JSON with "data" field: {"appId":"DECT_SHELL", "data":"actual output"}
     * - Plain text: actual shell output
     * - Nested JSON: recursively extracts content
     * 
     * IMPORTANT: Preserves all whitespace, indentation, and newlines in the output
     */
    private fun extractCleanMessage(message: String): String {
        if (message.isBlank()) {
            return ""
        }
        
        // Don't trim - preserve leading/trailing whitespace and all formatting
        // Only check if it's JSON by looking at first non-whitespace character
        val trimmedForCheck = message.trimStart()
        
        // Quick check: if it doesn't start with '{', it's likely plain text
        if (!trimmedForCheck.startsWith("{")) {
            // Return as-is to preserve all formatting
            return message
        }
        
        try {
            // Try to parse as JSON
            val jsonObject = org.json.JSONObject(trimmedForCheck)
            
            // Check for "data" field first (most common format for DECT_SHELL responses)
            if (jsonObject.has("data")) {
                val data = jsonObject.optString("data", "")
                if (data.isNotEmpty()) {
                    // Recursively extract in case data itself is JSON
                    val extracted = extractCleanMessage(data)
                    if (extracted.isNotEmpty()) {
                        return extracted
                    }
                }
            }
            
            // Check for other common fields that might contain the actual message
            val possibleFields = listOf("message", "text", "output", "response", "result", "content")
            for (field in possibleFields) {
                if (jsonObject.has(field)) {
                    val value = jsonObject.optString(field, "")
                    if (value.isNotEmpty()) {
                        val extracted = extractCleanMessage(value)
                        if (extracted.isNotEmpty()) {
                            return extracted
                        }
                    }
                }
            }
            
            // If it's valid JSON but we couldn't extract meaningful content,
            // return empty string to avoid showing JSON structure to user
            // This ensures users never see raw JSON formatting
            return ""
        } catch (e: org.json.JSONException) {
            // Not valid JSON, return as-is (this is the actual shell output)
            // Preserve all formatting
            return message
        } catch (e: Exception) {
            // Any other error, return as-is to be safe
            return message
        }
    }

    private fun writePrompt(message: String) {
        // Display with green prompt and white command text
        writeOutputWithPrompt(message)
    }

    private fun writeOutputWithPrompt(command: String) {
        runOnUiThread {
            val textView = TextView(this).apply {
                textSize = 14f
                typeface = Typeface.MONOSPACE
                setPadding(0, 2, 0, 2)
                setBackgroundColor(Color.BLACK) // Ensure black background
                
                // Create SpannableString to color prompt green and command white
                val fullText = "desh:~$ $command"
                val spannable = android.text.SpannableString(fullText)
                
                // Color the prompt part green
                val promptEnd = "desh:~$ ".length
                spannable.setSpan(
                    android.text.style.ForegroundColorSpan(Color.parseColor("#4CAF50")),
                    0,
                    promptEnd,
                    android.text.Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
                )
                
                // Color the command part white
                if (command.isNotEmpty()) {
                    spannable.setSpan(
                        android.text.style.ForegroundColorSpan(Color.WHITE),
                        promptEnd,
                        fullText.length,
                        android.text.Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
                    )
                }
                
                text = spannable
            }
            outputLayout.addView(textView)
            scrollView.post {
                scrollView.fullScroll(View.FOCUS_DOWN)
            }
        }
    }

    private fun writeOutput(message: String) {
        runOnUiThread {
            // Temporarily remove prompt if it exists, so output appears above it
            val promptIndex = if (lastPromptTextView != null) {
                val index = outputLayout.indexOfChild(lastPromptTextView)
                if (index >= 0) {
                    outputLayout.removeView(lastPromptTextView)
                    index
                } else {
                    -1
                }
            } else {
                -1
            }
            
            // Split message by newlines to preserve formatting
            // Each line will be a separate TextView to preserve indentation
            val lines = message.split("\n")
            
            lines.forEach { line ->
                val textView = TextView(this).apply {
                    text = line
                    textSize = 14f
                    typeface = Typeface.MONOSPACE
                    setTextColor(Color.WHITE) // White text on black background (terminal style)
                    setPadding(0, 2, 0, 2)
                    setBackgroundColor(Color.BLACK) // Ensure black background
                    // Preserve whitespace and formatting
                    // Use a monospace font which naturally preserves spacing
                }
                outputLayout.addView(textView)
            }
            
            // Re-add prompt at the bottom after the output
            if (promptIndex >= 0 && lastPromptTextView != null) {
                outputLayout.addView(lastPromptTextView)
            } else if (lastPromptTextView == null) {
                // Prompt was removed, create a new one
                showNewPrompt()
            }
            
            // Auto-scroll to bottom after adding output (to show prompt below)
            // Use postDelayed to ensure keyboard is handled and content is laid out
            scrollView.postDelayed({
                val scrollAmount = scrollView.getChildAt(0)?.height ?: 0
                scrollView.fullScroll(View.FOCUS_DOWN)
                // Force scrollbar to update
                // Force scroll to bottom to ensure latest content is visible
                scrollView.postDelayed({
                    if (scrollAmount > 0) {
                        scrollView.smoothScrollTo(0, scrollAmount)
                    }
                }, 50)
            }, 100)
        }
    }

    private fun writeError(message: String) {
        runOnUiThread {
            // Temporarily remove prompt if it exists, so error appears above it
            val promptIndex = if (lastPromptTextView != null) {
                val index = outputLayout.indexOfChild(lastPromptTextView)
                if (index >= 0) {
                    outputLayout.removeView(lastPromptTextView)
                    index
                } else {
                    -1
                }
            } else {
                -1
            }
            
            val textView = TextView(this).apply {
                text = message
                textSize = 14f
                typeface = Typeface.MONOSPACE
                setTextColor(Color.RED) // Red text for errors
                setPadding(0, 2, 0, 2)
                setBackgroundColor(Color.BLACK) // Ensure black background
            }
            outputLayout.addView(textView)
            
            // Re-add prompt at the bottom after the error
            if (promptIndex >= 0 && lastPromptTextView != null) {
                outputLayout.addView(lastPromptTextView)
            } else if (lastPromptTextView == null) {
                // Prompt was removed, create a new one
                showNewPrompt()
            }
            
            scrollView.post {
                scrollView.fullScroll(View.FOCUS_DOWN)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        client?.close()
    }
}
