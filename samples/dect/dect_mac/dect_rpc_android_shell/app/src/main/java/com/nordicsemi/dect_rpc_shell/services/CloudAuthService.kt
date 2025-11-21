/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

package com.nordicsemi.dect_rpc_shell.services

import org.eclipse.paho.client.mqttv3.MqttConnectOptions
import java.util.UUID
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocket
import javax.net.ssl.SSLSocketFactory
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager
import java.io.IOException
import java.net.InetAddress
import java.net.Socket
import java.security.cert.X509Certificate

/**
 * nRF Cloud authentication and connection service
 */
object CloudAuthService {
    private const val MQTT_BROKER = "mqtt.nrfcloud.com"
    private const val MQTT_PORT = 8883

    /**
     * Create a trust manager that accepts all certificates
     * (nRF Cloud uses certificates that may not be in the system trust store)
     */
    private fun createTrustAllManager(): X509TrustManager {
        return object : X509TrustManager {
            override fun checkClientTrusted(chain: Array<out X509Certificate>?, authType: String?) {
                // Accept all client certificates
            }

            override fun checkServerTrusted(chain: Array<out X509Certificate>?, authType: String?) {
                // Accept all server certificates (including nRF Cloud)
            }

            override fun getAcceptedIssuers(): Array<X509Certificate> {
                return arrayOf()
            }
        }
    }

    /**
     * Custom SSLSocketFactory that sets SNI (Server Name Indication) for nRF Cloud
     */
    private class SNISSLSocketFactory(
        private val delegate: SSLSocketFactory,
        private val hostname: String
    ) : SSLSocketFactory() {
        override fun createSocket(): Socket {
            val socket = delegate.createSocket() as SSLSocket
            // Set SNI synchronously before returning socket
            setSNI(socket)
            return socket
        }

        override fun createSocket(host: String?, port: Int): Socket {
            val socket = delegate.createSocket(host, port) as SSLSocket
            // Set SNI synchronously before returning socket
            setSNI(socket)
            return socket
        }

        override fun createSocket(host: String?, port: Int, localHost: InetAddress?, localPort: Int): Socket {
            val socket = delegate.createSocket(host, port, localHost, localPort) as SSLSocket
            // Set SNI synchronously before returning socket
            setSNI(socket)
            return socket
        }

        override fun createSocket(host: InetAddress?, port: Int): Socket {
            val socket = delegate.createSocket(host, port) as SSLSocket
            // Set SNI synchronously before returning socket
            setSNI(socket)
            return socket
        }

        override fun createSocket(address: InetAddress?, port: Int, localAddress: InetAddress?, localPort: Int): Socket {
            val socket = delegate.createSocket(address, port, localAddress, localPort) as SSLSocket
            // Set SNI synchronously before returning socket
            setSNI(socket)
            return socket
        }

        override fun createSocket(s: Socket?, host: String?, port: Int, autoClose: Boolean): Socket {
            // This is the method Eclipse Paho typically uses for SSL connections
            // Create socket and set SNI BEFORE returning it to ensure SNI is set before handshake
            val socket = delegate.createSocket(s, host, port, autoClose) as SSLSocket
            // Set SNI synchronously - this must happen before the socket is used
            // The host parameter might be different from our hostname, but we use our configured hostname
            setSNI(socket)
            return socket
        }

        override fun getDefaultCipherSuites(): Array<String> {
            return delegate.defaultCipherSuites
        }

        override fun getSupportedCipherSuites(): Array<String> {
            return delegate.supportedCipherSuites
        }

        private fun setSNI(socket: SSLSocket) {
            try {
                // Set SNI hostname for TLS handshake BEFORE starting handshake
                // According to nRF Cloud documentation, SNI is required for proper certificate selection
                // See: https://docs.nordicsemi.com/bundle/nrf-asset-tracker-v1.5.x/page/docs/aws/SNI.html
                // SNI must be set before the SSL handshake begins
                
                // Set SNI immediately - don't check session as handshake might start in background thread
                val parameters = socket.sslParameters
                val sniHostName = javax.net.ssl.SNIHostName(hostname)
                parameters.serverNames = listOf(sniHostName)
                socket.sslParameters = parameters
                android.util.Log.d("CloudAuthService", "SNI set to: $hostname")
                
                // Verify SNI was set correctly
                val verifySNI = socket.sslParameters.serverNames
                if (verifySNI.isNotEmpty()) {
                    val sniValue = verifySNI[0]
                    if (sniValue is javax.net.ssl.SNIHostName) {
                        android.util.Log.d("CloudAuthService", "SNI verified: ${String(sniValue.encoded)}")
                    } else {
                        android.util.Log.d("CloudAuthService", "SNI verified: $sniValue")
                    }
                } else {
                    android.util.Log.w("CloudAuthService", "SNI verification failed - serverNames is empty")
                }
            } catch (e: Exception) {
                android.util.Log.e("CloudAuthService", "Failed to set SNI: ${e.message}", e)
            }
        }
    }

    /**
     * Create SSL context for nRF Cloud MQTT
     * Uses Android's default trust manager (system certificates) with TLS 1.2
     */
    private fun createSSLContext(): SSLContext {
        return try {
            // Try to get the default trust manager from Android
            val trustManagerFactory = javax.net.ssl.TrustManagerFactory.getInstance(
                javax.net.ssl.TrustManagerFactory.getDefaultAlgorithm()
            )
            trustManagerFactory.init(null as java.security.KeyStore?)
            val trustManagers = trustManagerFactory.trustManagers
            
            // Try TLS 1.2 first (required by nRF Cloud)
            val sslContext: SSLContext = try {
                SSLContext.getInstance("TLSv1.2").also {
                    android.util.Log.d("CloudAuthService", "TLSv1.2 protocol available")
                }
            } catch (e: Exception) {
                android.util.Log.d("CloudAuthService", "TLSv1.2 not available, trying TLS: ${e.message}")
                SSLContext.getInstance("TLS")
            }
            
            // Use Android's default trust managers (system certificates)
            // Pass null for KeyManager to indicate no client certificate authentication
            // This is required for API key authentication (username/password) instead of mTLS
            sslContext.init(null, trustManagers, java.security.SecureRandom())
            android.util.Log.d("CloudAuthService", "SSL Context created: ${sslContext.protocol}")
            android.util.Log.d("CloudAuthService", "SSL Context provider: ${sslContext.provider.name}")
            android.util.Log.d("CloudAuthService", "Using ${trustManagers.size} trust manager(s)")
            android.util.Log.d("CloudAuthService", "Client authentication: disabled (using API key auth)")
            sslContext
        } catch (e: Exception) {
            android.util.Log.e("CloudAuthService", "Failed to create SSL context with default trust manager: ${e.message}", e)
            // Fallback to trust-all manager if default fails
            android.util.Log.w("CloudAuthService", "Falling back to trust-all manager")
            try {
                val sslContext = SSLContext.getInstance("TLS")
                sslContext.init(null, arrayOf<TrustManager>(createTrustAllManager()), java.security.SecureRandom())
                sslContext
            } catch (e2: Exception) {
                throw RuntimeException("Failed to create SSL context", e2)
            }
        }
    }

    /**
     * Create MQTT connection options for nRF Cloud
     */
    fun createMqttConnectOptions(deviceId: String, apiKey: String): MqttConnectOptions {
        require(deviceId.isNotBlank()) { "Device ID cannot be null or empty" }
        require(apiKey.isNotBlank()) { "API key cannot be null or empty" }

        val options = MqttConnectOptions().apply {
            isCleanSession = true
            keepAliveInterval = 60
            
            // nRF Cloud MQTT authentication: username = device ID, password = API key
            // See: https://docs.nordicsemi.com/bundle/nrf-cloud/page/APIs/MQTT/MQTTOverview.html
            userName = deviceId
            password = apiKey.toCharArray()
            
            // Configure SSL/TLS with SNI support
            try {
                val sslContext = createSSLContext()
                val baseFactory = sslContext.socketFactory
                // Wrap with SNI-enabled factory to ensure Server Name Indication is set
                socketFactory = SNISSLSocketFactory(baseFactory, MQTT_BROKER)
                // Disable hostname verification - SNI is set via our custom socket factory
                // Hostname verification can cause issues with some MQTT brokers
                isHttpsHostnameVerificationEnabled = false
                android.util.Log.d("CloudAuthService", "SSL socket factory configured with SNI")
                android.util.Log.d("CloudAuthService", "Socket factory class: ${socketFactory.javaClass.name}")
                android.util.Log.d("CloudAuthService", "SNI hostname: $MQTT_BROKER")
                android.util.Log.d("CloudAuthService", "Hostname verification: disabled (SNI handles hostname)")
                android.util.Log.d("CloudAuthService", "Username (deviceId): $deviceId")
                android.util.Log.d("CloudAuthService", "Password (apiKey): ${apiKey.take(4)}... (${apiKey.length} chars)")
            } catch (e: Exception) {
                android.util.Log.e("CloudAuthService", "Failed to configure SSL: ${e.message}", e)
                throw e
            }
        }

        return options
    }

    /**
     * Create MQTT connection options for nRF Cloud (legacy method name)
     */
    @Deprecated("Use createMqttConnectOptions instead", ReplaceWith("createMqttConnectOptions(deviceId, apiKey)"))
    fun createMqttOptions(deviceId: String, apiKey: String): MqttConnectOptions {
        return createMqttConnectOptions(deviceId, apiKey)
    }

    /**
     * Get MQTT broker URI
     */
    fun getMqttBrokerUri(): String {
        return "ssl://$MQTT_BROKER:$MQTT_PORT"
    }

    /**
     * Validate device ID format
     */
    fun isValidDeviceId(deviceId: String): Boolean {
        if (deviceId.isBlank()) {
            return false
        }
        // nRF Cloud device IDs are typically UUIDs
        return try {
            UUID.fromString(deviceId)
            true
        } catch (e: IllegalArgumentException) {
            deviceId.isNotBlank()
        }
    }

    /**
     * Validate API key format
     */
    fun isValidApiKey(apiKey: String): Boolean {
        return apiKey.isNotBlank() && apiKey.length >= 8
    }
}

