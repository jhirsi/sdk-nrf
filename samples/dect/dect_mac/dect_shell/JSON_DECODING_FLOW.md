# JSON Decoding Flow in dect_shell

## Overview

This document explains how JSON messages are handled in `dect_shell` when received from nRF Cloud.

## Who Decodes JSON?

### nRF Cloud Library
- **Does NOT decode JSON** for data channel messages
- For `NRF_CLOUD_EVT_RX_DATA_GENERAL` events, nRF Cloud passes **raw MQTT payload bytes** to the application
- JSON decoding only happens for **control channel (CC) messages** (shadow updates), not for data channel (DC) messages

### dect_shell Application
- **dect_shell decodes JSON itself** using cJSON library
- Two functions handle JSON parsing:
  1. `cloud_shell_parse_dect_rpc_json()` - for `appId: "DECT_RPC"`
  2. `cloud_shell_parse_desh_cmd()` - for `appId: "DECT_SHELL"`

## Message Flow

```
MQTT Message Arrives
    ↓
nRF Cloud Transport (nrf_cloud_transport.c)
    ↓
Raw payload stored in nct.payload_buf (no JSON decoding)
    ↓
dc_rx_data_handler() in nrf_cloud_fsm.c
    ↓
Sets cloud_evt.data = raw MQTT payload bytes
    ↓
Triggers NRF_CLOUD_EVT_RX_DATA_GENERAL event
    ↓
dect_shell event handler (cloud_mqtt_shell.c)
    ↓
Checks if data starts with '{' (JSON indicator)
    ↓
Parses JSON using cJSON
    ↓
Checks appId field:
    - "DECT_RPC" → Decode base64 data → Pass to RPC transport
    - "DECT_SHELL" → Extract command string → Execute shell command
```

## DECT_RPC AppId Handling

### Current Implementation

The `dect_shell` correctly handles `DECT_RPC` appId:

1. **Raw Binary Check** (line 259):
   - First checks if data is raw binary RPC packet
   - Uses `dect_rpc_mqtt_transport_handle_rx_data()` which checks if data looks like RPC packet

2. **JSON Check** (line 266):
   - If data starts with `'{'`, it's likely JSON
   - Calls `cloud_shell_parse_dect_rpc_json()` which:
     - Parses JSON using `cJSON_Parse()`
     - Checks if `appId == "DECT_RPC"`
     - Extracts `data` field (base64 string)
     - Decodes base64 to binary using `base64_decode()`
     - Returns decoded binary data

3. **RPC Processing** (line 273):
   - Passes decoded binary to `dect_rpc_mqtt_transport_handle_rx_data()`
   - If handled successfully, breaks (stops further processing)

4. **Fallback** (line 284):
   - If not DECT_RPC, checks for DECT_SHELL commands

### Code Flow

```c
case NRF_CLOUD_EVT_RX_DATA_GENERAL:
    // Step 1: Try raw binary RPC packet
    if (dect_rpc_mqtt_transport_handle_rx_data(&evt->data)) {
        break; // Handled
    }

    // Step 2: Try JSON with base64-encoded RPC data
    if (evt->data.ptr[0] == '{') {
        struct nrf_cloud_data decoded_data;
        if (cloud_shell_parse_dect_rpc_json(evt->data.ptr, &decoded_data)) {
            // JSON parsed, base64 decoded
            if (dect_rpc_mqtt_transport_handle_rx_data(&decoded_data)) {
                k_free(decoded_data.ptr);
                break; // Handled
            }
            k_free(decoded_data.ptr);
        }
    }

    // Step 3: Try DECT_SHELL commands
    if (evt->data.ptr[0] == '{') {
        if (cloud_shell_parse_desh_cmd(evt->data.ptr)) {
            // Execute shell command
        }
    }
    break;
```

## JSON Format Expected

### DECT_RPC Format
```json
{
  "appId": "DECT_RPC",
  "data": "gR3/AAABgA=="
}
```

Where `data` is a base64-encoded string containing the binary RPC packet.

### DECT_SHELL Format
```json
{
  "appId": "DECT_SHELL",
  "data": "version"
}
```

Where `data` is a shell command string.

## Verification

✅ **Correct Implementation:**
- JSON is parsed correctly using cJSON
- `appId` field is checked case-sensitively
- Base64 decoding uses Zephyr's `base64_decode()` function
- Memory is properly allocated and freed
- Error handling is in place

✅ **Handling Order:**
1. Raw binary RPC (fastest path)
2. JSON with DECT_RPC appId
3. JSON with DECT_SHELL appId

## Potential Improvements

1. **Efficiency**: The code checks `evt->data.ptr[0] == '{'` twice. Could optimize by checking once and branching.

2. **Error Logging**: Could add more detailed error logging for debugging JSON parsing failures.

3. **Validation**: Could add validation for base64 string format before decoding.

## Conclusion

**Yes, `dect_shell` handles `DECT_RPC` appId correctly.** The JSON decoding is done by `dect_shell` itself using cJSON, and the implementation properly:
- Parses JSON
- Checks appId
- Decodes base64
- Passes binary to RPC transport

nRF Cloud library does not decode JSON for data channel messages - it only passes raw bytes to the application.

