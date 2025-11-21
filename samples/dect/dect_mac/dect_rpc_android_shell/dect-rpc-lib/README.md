# DECT RPC Library

This is an Android library module that provides the DECT RPC (Remote Procedure Call) API for communicating with DECT NR+ devices via nRF Cloud MQTT.

## Overview

The library implements the nRF RPC protocol over MQTT, allowing Android applications to:
- Connect to nRF Cloud MQTT
- Send DECT RPC commands (activate, deactivate, get status, read/write settings)
- Subscribe to DECT events
- Handle command/response correlation using context IDs

## Package

All classes are in the `com.nordicsemi.dect_rpc` package.

## Main Classes

- **`DectRpcClient`** - Main RPC client class for MQTT communication
- **`DectRpcIds`** - Command and event ID enumerations
- **`DectDataStructures`** - Data classes for DECT settings, status, events
- **`CborSerializer`** - CBOR serialization/deserialization for DECT structures
- **`NrfRpcProtocol`** - nRF RPC protocol encoding/decoding

## Dependencies

- **Eclipse Paho MQTT** - MQTT client library
- **CBOR (co.nstant.in:cbor)** - CBOR encoding/decoding
- **Kotlin Coroutines** - For async operations

## Usage

```kotlin
import com.nordicsemi.dect_rpc.DectRpcClient
import org.eclipse.paho.client.mqttv3.MqttConnectOptions

// Create client
val client = DectRpcClient()

// Set up event handlers
client.onConnectionStateChanged = { connected ->
    println("Connection state: $connected")
}

client.onEventReceived = { event ->
    println("Event received: ${event.eventId}")
}

// Connect
val mqttOptions = MqttConnectOptions().apply {
    userName = deviceId
    password = apiKey.toCharArray()
}
client.connect(deviceId, mqttOptions)

// Use suspend functions for commands
val status = client.getStatusInfo(0)
client.activate(0)
client.deactivate(0)
```

## Migration from C# Library

This library is the Kotlin conversion of the original C# `DectNrpRpcNetCloudLib` library. The C# library at `/nrf/subsys/net/lib/dect_nrp_rpc_net_cloud_lib/` is no longer used and has been replaced by this Android library module.

