# DECT RPC Cloud Test Examples

This document provides hex byte examples for testing DECT RPC commands via nRF Cloud MQTT.

## Packet Format

nRF RPC packets consist of a 5-byte header followed by a CBOR payload:

### Header Format (5 bytes):
- **Byte 0**: Packet type (0x80 for command) | Source context ID (7 bits)
- **Byte 1**: Command ID
- **Byte 2**: Destination context ID (0xFF = unknown initially)
- **Byte 3**: Source group ID (0x00 for dect_rpc group)
- **Byte 4**: Destination group ID (0x00 for dect_rpc group)

### CBOR Payload:
- Encoded as CBOR array/values according to command requirements

## Command IDs

From `dect_rpc_ids.h`:
- `DECT_RPC_CMD_ACTIVATE = 0`
- `DECT_RPC_CMD_DEACTIVATE = 1`
- `DECT_RPC_CMD_STATUS_INFO_GET = 17`
- `DECT_RPC_CMD_SETTINGS_READ = 15`
- `DECT_RPC_CMD_SETTINGS_WRITE = 16`
- `DECT_RPC_CMD_EVENT_SUBSCRIBE = 21`

## Example: "dect status" (Status Info Get)

### Command: `DECT_RPC_CMD_STATUS_INFO_GET` (17 = 0x11)

**Request Format:**
- Command expects: `[iface_index]` as CBOR array
- For iface_index = 0:
  - CBOR array of length 1: `0x81` (major type 4, length 1)
  - CBOR integer 0: `0x00` (major type 0, value 0)

**Hex Bytes (with context ID = 1):**

```
81 11 FF 00 00 81 00
```

**Breakdown:**
- `81` = 0x80 (command) | 0x01 (context ID 1)
- `11` = 17 (DECT_RPC_CMD_STATUS_INFO_GET)
- `FF` = 0xFF (destination context ID, unknown)
- `00` = Source group ID (dect_rpc = 0)
- `00` = Destination group ID (dect_rpc = 0)
- `81 00` = CBOR array[1] containing integer 0

**Alternative (with context ID = 0):**

```
80 11 FF 00 00 81 00
```

### Response Format:

The server responds with:
- Response packet type: `0x01`
- Same command ID: `0x11` (17)
- Destination context ID: `0x01` (matches source context ID from request)
- Payload: `[result_code (int), status_info (CBOR array)]`

**Example Response (success):**

```
01 11 01 00 00 [CBOR array with result_code=0 and status_info]
```

## Example: "dect activate" (Activate)

### Command: `DECT_RPC_CMD_ACTIVATE` (0 = 0x00)

**Request Format:**
- Command expects: `[iface_index]` as CBOR array
- For iface_index = 0: `81 00`

**Hex Bytes (with context ID = 1):**

```
81 00 FF 00 00 81 00
```

**Breakdown:**
- `81` = 0x80 (command) | 0x01 (context ID 1)
- `00` = 0 (DECT_RPC_CMD_ACTIVATE)
- `FF` = 0xFF (destination context ID)
- `00` = Source group ID
- `00` = Destination group ID
- `81 00` = CBOR array[1] containing integer 0

## Example: "dect settings read" (Settings Read)

### Command: `DECT_RPC_CMD_SETTINGS_READ` (27 = 0x1B)

**Request Format:**
- Command expects: `[iface_index]` as CBOR array
- For iface_index = 0: `81 00`

**Hex Bytes (with context ID = 1):**

```
81 1B FF 00 00 81 00
```

## Example: "dect settings write" (Settings Write)

### Command: `DECT_RPC_CMD_SETTINGS_WRITE` (28 = 0x1C)

**Request Format:**
- Command expects: `[iface_index, settings]` as CBOR array
- Settings is a CBOR array with many fields

**Hex Bytes (with context ID = 1, iface_index = 0, minimal settings):**

```
81 1C FF 00 00 [CBOR array: iface_index=0, settings_array]
```

The settings array is complex and contains ~30+ fields. See `CborSerializer.EncodeSettings()` for full structure.

## Initialization (Handshake)

### Is INIT Packet Required?

**For NOWAIT groups (MQTT transport):** The server uses `NRF_RPC_GROUP_DEFINE_NOWAIT()`, which means:
- The server doesn't block waiting for INIT packets
- **Commands can be sent directly** without INIT packet
- However, **sending an INIT packet first is recommended** for proper group binding

### INIT Packet Format

**Header (5 bytes):**
- Byte 0: `0x04` (INIT packet type)
- Byte 1: `0x00` (not used for INIT)
- Byte 2: `0xFF` (destination context ID, unknown)
- Byte 3: `0x00` (source group ID - client's group ID, typically 0)
- Byte 4: `0xFF` (destination group ID, unknown initially)

**Payload:**
- Byte 0: Max protocol version (currently `0x00`)
- Byte 1: Min protocol version (currently `0x00`)
- Bytes 2+: Group name string: `"dect_rpc"` (8 bytes, no null terminator)

**Complete INIT Packet (hex):**
```
04 00 FF 00 FF 00 00 64 65 63 74 5F 72 70 63
```

**Breakdown:**
- `04` = INIT packet type
- `00` = Not used
- `FF` = Destination context ID (unknown)
- `00` = Source group ID (client group = 0)
- `FF` = Destination group ID (unknown)
- `00 00` = Protocol version (min=0, max=0)
- `64 65 63 74 5F 72 70 63` = "dect_rpc" in ASCII

**Server Response:**
The server will respond with an INIT packet containing its group ID. This establishes the binding.

## Testing via nRF Cloud

### Option 1: nRF Cloud Web Terminal

The nRF Cloud web interface terminal typically sends JSON messages, but you can send raw binary data using base64 encoding:

1. **Access nRF Cloud Terminal:**
   - Log in to [nRF Cloud](https://nrfcloud.com/)
   - Navigate to your device
   - Open the **Terminal** tab

2. **Convert Hex to Base64:**
   - Convert your hex bytes to base64
   - Example: `81 11 FF 00 00 81 00` → `gRH/AAABgA==`

3. **Send as JSON (if terminal requires JSON):**
   ```json
   {
     "appId": "DECT_RPC",
     "data": "gRH/AAABgA=="
   }
   ```
   Note: This requires the server to decode base64. The current implementation expects raw binary.

4. **Alternative: Use MQTT Client Tools** (see Option 2 below) - Recommended for raw binary data

### Option 2: MQTT Client Tools (Recommended for Raw Binary)

1. **Connect to nRF Cloud MQTT** using your device credentials
2. **Publish to device topic**: `{device_id}/c2d`
3. **Subscribe to device topic**: `{device_id}/d2c` (for responses)
4. **Recommended**: Send INIT packet first, then send commands
5. **Alternative**: Send commands directly (may work but INIT is recommended)
6. **Send hex bytes** as binary payload (not as hex string!)

### Example using MQTT client:

```bash
# Step 1: Send INIT packet (recommended)
mosquitto_pub -h mqtt.nrfcloud.com -p 8883 \
  --cafile ca.pem --cert client.pem --key client.key \
  -t "YOUR_DEVICE_ID/c2d" \
  -m "$(echo '04 00 FF 00 FF 00 00 64 65 63 74 5F 72 70 63' | xxd -r -p)"

# Wait a moment for server response, then send command
sleep 1

# Step 2: Publish "dect status" command
mosquitto_pub -h mqtt.nrfcloud.com -p 8883 \
  --cafile ca.pem --cert client.pem --key client.key \
  -t "YOUR_DEVICE_ID/c2d" \
  -m "$(echo '81 11 FF 00 00 81 00' | xxd -r -p)"
```

### Example using Python:

```python
import paho.mqtt.client as mqtt
import binascii
import time

# Hex bytes for INIT packet
init_hex = "04 00 FF 00 FF 00 00 64 65 63 74 5F 72 70 63"
init_payload = binascii.unhexlify(init_hex.replace(" ", ""))

# Hex bytes for "dect status" command
cmd_hex = "81 11 FF 00 00 81 00"
cmd_payload = binascii.unhexlify(cmd_hex.replace(" ", ""))

client = mqtt.Client()
client.tls_set(ca_certs="ca.pem", certfile="client.pem", keyfile="client.key")
client.connect("mqtt.nrfcloud.com", 8883, 60)

# Subscribe for responses first
client.subscribe("YOUR_DEVICE_ID/d2c", qos=1)

# Step 1: Send INIT packet (recommended)
client.publish("YOUR_DEVICE_ID/c2d", init_payload, qos=1)
time.sleep(1)  # Wait for server INIT response

# Step 2: Publish command
client.publish("YOUR_DEVICE_ID/c2d", cmd_payload, qos=1)

# Keep connection alive to receive responses
client.loop_start()
time.sleep(5)
client.loop_stop()
```

## Notes

1. **INIT Packet**: While not strictly required for NOWAIT groups, sending an INIT packet first is **recommended** for proper group binding and reliable command/response matching.

2. **Context ID**: Use a unique context ID (1-127) for each command. The server will use this in the response to match requests.

3. **CBOR Encoding**: The payload uses CBOR encoding. Integer 0 is `0x00`, array of length 1 is `0x81`.

4. **Binary Payload**: Send as binary bytes, not as hex string!

5. **Response Matching**: The server responds with the same context ID in byte 2 of the response header.

6. **Error Responses**: If command fails, response payload will be `[error_code]` where error_code < 0.

7. **Direct Commands**: You can send commands directly without INIT, but the server may treat them as events if group binding isn't established. INIT packet ensures proper command/response handling.

## Converting Hex to Base64 (for Web Terminal)

If you need to send via nRF Cloud web terminal which may require base64:

**Using command line:**
```bash
# Convert hex string to base64
echo "81 11 FF 00 00 81 00" | xxd -r -p | base64
# Output: gRH/AAABgA==
```

**Using Python:**
```python
import binascii
import base64

hex_str = "81 11 FF 00 00 81 00"
hex_bytes = binascii.unhexlify(hex_str.replace(" ", ""))
base64_str = base64.b64encode(hex_bytes).decode('ascii')
print(base64_str)  # Output: gRH/AAABgA==
```

**Converting back from base64:**
```bash
# Base64 to hex
echo "gRH/AAABgA==" | base64 -d | xxd -p
# Output: 8111ff00008100
```

**Note:** The current DECT RPC server implementation expects raw binary data via `NRF_CLOUD_EVT_RX_DATA_GENERAL`. If the web terminal sends JSON, you may need to modify the server to decode base64 from JSON, or use MQTT client tools instead.

## Troubleshooting

- **No Response**: Check that the device is connected to nRF Cloud and RPC server is initialized
- **Invalid Packet**: Verify hex bytes match the format exactly
- **Decode Error**: Check that CBOR payload matches expected format
- **Timeout**: Server may take time to process; wait up to 30 seconds
- **Web Terminal Issues**: If web terminal only supports JSON, use MQTT client tools (mosquitto_pub, Python, etc.) for raw binary data

