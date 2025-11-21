# Sending Raw Binary Data from nRF Cloud Terminal

## Overview

The nRF Cloud web terminal interface typically sends JSON-formatted messages. However, DECT RPC requires raw binary data (hex bytes). This guide explains how to send raw binary data from the nRF Cloud terminal.

## Method 1: Direct Binary (if supported)

Some nRF Cloud terminal implementations may support direct binary input. Check if your terminal has a "Binary" or "Hex" input mode.

## Method 2: Base64 Encoding (Recommended for Web Terminal)

If the terminal only accepts text, encode your hex bytes as base64:

### Step 1: Convert Hex to Base64

**Example: "dect status" command**
- Hex: `81 1D FF 00 00 81 00`
- Base64: `gR3/AAABgA==`

**Using command line:**
```bash
echo "81 1D FF 00 00 81 00" | xxd -r -p | base64
```

**Using Python:**
```python
import binascii
import base64

hex_str = "81 1D FF 00 00 81 00"
hex_bytes = binascii.unhexlify(hex_str.replace(" ", ""))
base64_str = base64.b64encode(hex_bytes).decode('ascii')
print(base64_str)  # gR3/AAABgA==
```

### Step 2: Send via nRF Cloud Terminal

**Option A: If terminal accepts base64 directly:**
```
gR3/AAABgA==
```

**Option B: If terminal requires JSON:**
```json
{
  "appId": "DECT_RPC",
  "data": "gR3/AAABgA=="
}
```

### Step 3: Server-Side Base64 Decoding

**IMPORTANT:** The current DECT RPC server implementation expects raw binary data. If you send base64 via JSON, you need to modify the server to:

1. Detect base64-encoded data in JSON
2. Decode base64 to binary
3. Pass binary data to `dect_rpc_mqtt_transport_handle_rx_data()`

**Example server modification:**
```c
case NRF_CLOUD_EVT_RX_DATA_GENERAL:
    /* Check if it's JSON with base64 data */
    if (((char *)evt->data.ptr)[0] == '{') {
        cJSON *json = cJSON_Parse(evt->data.ptr);
        if (json) {
            cJSON *app_id = cJSON_GetObjectItem(json, "appId");
            cJSON *data = cJSON_GetObjectItem(json, "data");
            
            if (cJSON_IsString(app_id) && strcmp(app_id->valuestring, "DECT_RPC") == 0 &&
                cJSON_IsString(data)) {
                /* Decode base64 to binary */
                size_t decoded_len;
                uint8_t *decoded = base64_decode(data->valuestring, &decoded_len);
                if (decoded) {
                    struct nrf_cloud_data binary_data = {
                        .ptr = decoded,
                        .len = decoded_len
                    };
                    if (dect_rpc_mqtt_transport_handle_rx_data(&binary_data)) {
                        /* Handled */
                    }
                    k_free(decoded);
                }
                cJSON_Delete(json);
                break;
            }
            cJSON_Delete(json);
        }
    }
    /* Fall through to normal JSON handling */
    break;
```

## Method 3: Use MQTT Client Tools (Easiest)

Instead of the web terminal, use MQTT client tools that support raw binary:

### Using mosquitto_pub:
```bash
# Send "dect status" command
echo "81 1D FF 00 00 81 00" | xxd -r -p | \
  mosquitto_pub -h mqtt.nrfcloud.com -p 8883 \
    --cafile ca.pem --cert client.pem --key client.key \
    -t "YOUR_DEVICE_ID/c2d" -s
```

### Using Python:
```python
import paho.mqtt.client as mqtt
import binascii

hex_str = "81 1D FF 00 00 81 00"
binary = binascii.unhexlify(hex_str.replace(" ", ""))

client = mqtt.Client()
client.tls_set(ca_certs="ca.pem", certfile="client.pem", keyfile="client.key")
client.connect("mqtt.nrfcloud.com", 8883, 60)
client.publish("YOUR_DEVICE_ID/c2d", binary, qos=1)
```

## Quick Reference: Common Commands as Base64

| Command | Hex Bytes | Base64 |
|---------|-----------|--------|
| INIT | `04 00 FF 00 FF 00 00 64 65 63 74 5F 72 70 63` | `BAAA/wAA/wAAAGRlY3RfcnBj` |
| Activate | `81 00 FF 00 00 81 00` | `gQD/AAABgA==` |
| Deactivate | `81 01 FF 00 00 81 00` | `gQH/AAABgA==` |
| Status | `81 1D FF 00 00 81 00` | `gR3/AAABgA==` |
| Settings Read | `81 1B FF 00 00 81 00` | `gRv/AAABgA==` |

## Testing Steps

1. **Convert your hex bytes to base64** (see examples above)
2. **Open nRF Cloud web terminal** for your device
3. **Send base64 string** (or JSON with base64 data field)
4. **Check device logs** to see if data is received
5. **If using JSON**, ensure server decodes base64 before processing

## Troubleshooting

- **No response**: Verify device is connected and RPC server is initialized
- **Invalid format**: Ensure base64 encoding is correct
- **Server not receiving**: Check if server handles JSON/base64 or only raw binary
- **Use MQTT tools**: If web terminal is problematic, use mosquitto_pub or Python MQTT client

