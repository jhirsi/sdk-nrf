/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc_shell.activities

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.nordicsemi.dect_rpc_shell.services.CloudAuthService
import com.nordicsemi.dect_rpc_shell.services.CloudRestService
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext

class LoginActivity : AppCompatActivity() {
    private lateinit var apiKeyInput: EditText
    private lateinit var loginButton: Button
    private lateinit var progressBar: ProgressBar
    private lateinit var statusText: TextView
    private var isAuthenticating = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        try {
            // Check if already logged in (stored API key)
            val prefs = getSharedPreferences("DectRpcPrefs", MODE_PRIVATE)
            val savedApiKey = prefs.getString("ApiKey", null)

            if (!savedApiKey.isNullOrEmpty()) {
                // Skip login, go directly to device list
                navigateToDeviceList(savedApiKey)
                return
            }

            createUI()
        } catch (ex: Exception) {
            // Log error and show message
            android.util.Log.e("LoginActivity", "OnCreate failed: ${ex.message}", ex)
            Toast.makeText(this, "Error starting app: ${ex.message}", Toast.LENGTH_LONG).show()
            finish()
        }
    }

    private fun createUI() {
        // Main layout with proper padding to avoid system UI overlap
        val mainLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            // Add top padding to account for status bar/system UI
            setPadding(32, 80, 32, 32)
            // Enable fitsSystemWindows to handle system bars properly
            fitsSystemWindows = true
        }

        // Title
        val titleText = TextView(this).apply {
            text = "nRF Cloud Authentication"
            textSize = 24f
            gravity = android.view.Gravity.CENTER
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = 32
            }
        }
        mainLayout.addView(titleText)

        // Instructions
        val instructionsText = TextView(this).apply {
            text = "Enter your nRF Cloud API key to continue.\n\nYou can find your API key in the nRF Cloud portal under Account Settings."
            textSize = 14f
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = 24
            }
        }
        mainLayout.addView(instructionsText)

        // API Key input
        val apiKeyLabel = TextView(this).apply {
            text = "API Key:"
            textSize = 16f
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = 8
            }
        }
        mainLayout.addView(apiKeyLabel)

        apiKeyInput = EditText(this).apply {
            hint = "Enter your nRF Cloud API key"
            inputType = android.text.InputType.TYPE_CLASS_TEXT or android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = 16
            }
        }
        mainLayout.addView(apiKeyInput)

        // Status text
        statusText = TextView(this).apply {
            text = ""
            textSize = 14f
            gravity = android.view.Gravity.CENTER
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = 16
            }
        }
        mainLayout.addView(statusText)

        // Progress bar
        progressBar = ProgressBar(this).apply {
            isIndeterminate = true
            visibility = View.GONE
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = 16
            }
        }
        mainLayout.addView(progressBar)

        // Login button
        loginButton = Button(this).apply {
            text = "Login"
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            setOnClickListener { onLoginButtonClick() }
        }
        mainLayout.addView(loginButton)

        setContentView(mainLayout)
    }

    private fun onLoginButtonClick() {
        if (isAuthenticating) {
            return
        }

        val apiKey = apiKeyInput.text?.toString()?.trim()
        if (apiKey.isNullOrEmpty()) {
            statusText.text = "Please enter an API key"
            statusText.setTextColor(android.graphics.Color.RED)
            return
        }

        if (!CloudAuthService.isValidApiKey(apiKey)) {
            statusText.text = "Invalid API key format"
            statusText.setTextColor(android.graphics.Color.RED)
            return
        }

        isAuthenticating = true
        loginButton.isEnabled = false
        progressBar.visibility = View.VISIBLE
        statusText.text = "Validating API key..."
        statusText.setTextColor(android.graphics.Color.BLUE)

        CoroutineScope(Dispatchers.Main).launch {
            try {
                // Validate API key by making a test request
                val isValid = withContext(Dispatchers.IO) {
                    CloudRestService.validateApiKey(apiKey)
                }

                if (isValid) {
                    // Save API key for future use
                    val prefs = getSharedPreferences("DectRpcPrefs", MODE_PRIVATE)
                    prefs.edit().putString("ApiKey", apiKey).apply()

                    statusText.text = "Authentication successful!"
                    statusText.setTextColor(android.graphics.Color.GREEN)

                    // Navigate to device list
                    delay(500)
                    navigateToDeviceList(apiKey)
                } else {
                    statusText.text = "Invalid API key. Please check and try again."
                    statusText.setTextColor(android.graphics.Color.RED)
                }
            } catch (ex: Exception) {
                statusText.text = "Authentication failed: ${ex.message}"
                statusText.setTextColor(android.graphics.Color.RED)
            } finally {
                isAuthenticating = false
                loginButton.isEnabled = true
                progressBar.visibility = View.GONE
            }
        }
    }

    private fun navigateToDeviceList(apiKey: String) {
        val intent = Intent(this, DeviceListActivity::class.java)
        intent.putExtra("ApiKey", apiKey)
        startActivity(intent)
        finish() // Don't allow back to login screen
    }
}

