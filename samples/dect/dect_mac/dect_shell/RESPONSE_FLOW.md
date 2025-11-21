# DECT RPC Response Flow to Cloud Client

## Overview

This document explains how DECT RPC responses are sent back to the cloud client after a command is received and processed.

## Complete Flow Diagram

```
Cloud Client (MQTT)
    ↓
Publishes command to: {device_id}/c2d
    ↓
nRF Cloud MQTT Transport
    ↓
Receives MQTT message → stores raw payload
    ↓
Triggers: NRF_CLOUD_EVT_RX_DATA_GENERAL
    ↓
dect_shell event handler
    ↓
dect_rpc_mqtt_transport_handle_rx_data()
    ↓
nRF RPC receive handler
    ↓
DECT RPC Server command handler
    ↓
Processes command (e.g., dect_rpc_cmd_status_info_get)
    ↓
Calls: nrf_rpc_rsp_send_*() or nrf_rpc_rsp()
    ↓
nRF RPC library: nrf_rpc_rsp()
    ↓
Creates response packet with header:
  - Packet type: RPC_PACKET_TYPE_RSP (0x01)
  - Destination context ID: from command's source context ID
  - Command ID: same as request
    ↓
Calls transport: mqtt_transport_send()
    ↓
dect_rpc_mqtt_transport.c: mqtt_transport_send()
    ↓
Calls: nrf_cloud_send() with NRF_CLOUD_TOPIC_MESSAGE
    ↓
nRF Cloud: nct_dc_send()
    ↓
Publishes to MQTT topic: {device_id}/d2c
    ↓
Cloud Client receives response
```

## Detailed Steps

### 1. Command Reception (Cloud → Device)

**Topic**: `{device_id}/c2d` (cloud-to-device)

**Format**: Raw binary nRF RPC packet
- 5-byte header
- CBOR payload

**Example**: "dect status" command
```
81 1D FF 00 00 81 00
```

### 2. Command Processing

The DECT RPC server processes the command in its handler:

```c
static void dect_rpc_cmd_status_info_get(const struct nrf_rpc_group *group,
                                         struct nrf_rpc_cbor_ctx *ctx,
                                         void *handler_data)
{
    // ... decode parameters ...
    
    // Process command and get status info
    struct dect_status_info status;
    // ... populate status ...
    
    // Send response
    nrf_rpc_rsp_send_cbor(group, encode_status_info, &status);
}
```

### 3. Response Generation

**nRF RPC Library** (`nrf_rpc.c`):

```c
int nrf_rpc_rsp(const struct nrf_rpc_group *group, uint8_t *packet, size_t len)
{
    struct nrf_rpc_cmd_ctx *cmd_ctx = cmd_ctx_get_current();
    
    // Create response header
    hdr.dst = cmd_ctx->remote_id;  // Context ID from command
    hdr.type = NRF_RPC_PACKET_TYPE_RSP;  // 0x01
    hdr.id = NRF_RPC_ID_UNKNOWN;
    hdr.src_group_id = group->data->src_group_id;
    hdr.dst_group_id = group->data->dst_group_id;
    
    // Encode header
    header_encode(full_packet, &hdr);
    
    // Send via transport
    err = send(group, full_packet, len + NRF_RPC_HEADER_SIZE);
    
    return err;
}
```

**Response Header Format**:
- Byte 0: `0x01` (RPC_PACKET_TYPE_RSP) | (source context ID & 0x7F)
- Byte 1: Command ID (same as request)
- Byte 2: Destination context ID (from request's source context ID)
- Byte 3: Source group ID (0x00 for dect_rpc)
- Byte 4: Destination group ID (0x00 for dect_rpc)

### 4. Transport Layer

**MQTT Transport** (`dect_rpc_mqtt_transport.c`):

```c
static int mqtt_transport_send(const struct nrf_rpc_tr *transport,
                                const uint8_t *data, size_t length)
{
    // Use nRF Cloud send API
    struct nrf_cloud_tx_data tx_data = {
        .topic_type = NRF_CLOUD_TOPIC_MESSAGE,  // Uses d2c topic
        .data = {
            .ptr = (void *)data,
            .len = length
        },
        .qos = MQTT_QOS_1_AT_LEAST_ONCE
    };
    
    ret = nrf_cloud_send(&tx_data);
    return ret;
}
```

### 5. nRF Cloud Publishing

**nRF Cloud** (`nrf_cloud.c`):

```c
case NRF_CLOUD_TOPIC_MESSAGE: {
    if (msg->qos == MQTT_QOS_1_AT_LEAST_ONCE) {
        err = nct_dc_send(&dc_data);  // Publishes to d2c topic
    }
    break;
}
```

**Topic**: `{device_id}/d2c` (device-to-cloud)

**Format**: Same as received - raw binary nRF RPC packet

### 6. Response Reception (Device → Cloud)

**Topic**: `{device_id}/d2c`

**Response Format**:
- Header: 5 bytes (response packet type + context IDs)
- Payload: CBOR-encoded response data

**Example Response** (for "dect status"):
```
01 1D 01 00 00 [CBOR array with result_code=0 and status_info]
```

**Breakdown**:
- `01` = Response packet type (0x01) | Context ID 1
- `1D` = Command ID 29 (STATUS_INFO_GET)
- `01` = Destination context ID (matches request's source context ID)
- `00` = Source group ID
- `00` = Destination group ID
- `[CBOR...]` = Response payload: `[result_code, status_info]`

## Response Payload Format

### Success Response

CBOR array: `[result_code, data]`
- `result_code`: Integer (0 = success)
- `data`: CBOR-encoded response data (varies by command)

**Example** (Status Info Get):
```cbor
[0, [true, true, 25, false, 1, [...associations...], 0, [...children...], ...]]
```

### Error Response

CBOR array: `[error_code]`
- `error_code`: Integer (< 0 for errors)

**Example**:
```cbor
[-1]  // Generic error
[-EINVAL]  // Invalid parameter
```

## Context ID Matching

The response uses the **destination context ID** to match the original request:

1. **Request** (from cloud):
   - Source context ID: `0x01` (in byte 0)
   - Command ID: `0x1D` (in byte 1)

2. **Response** (to cloud):
   - Packet type: `0x01` (response) | Source context ID: `0x01` (server's context)
   - Command ID: `0x1D` (same as request)
   - **Destination context ID: `0x01`** (matches request's source context ID)

The cloud client uses this destination context ID to correlate responses with pending commands.

## MQTT Topics Summary

| Direction | Topic | Purpose |
|-----------|-------|---------|
| Cloud → Device | `{device_id}/c2d` | Commands from cloud |
| Device → Cloud | `{device_id}/d2c` | Responses and events to cloud |

## Code Locations

### Response Generation
- **nRF RPC**: `nrfxlib/nrf_rpc/nrf_rpc.c` - `nrf_rpc_rsp()`
- **DECT RPC Server**: `nrf/subsys/net/l2_dect_nrp/rpc/server/dect_rpc_server.c`
  - Each command handler calls `nrf_rpc_rsp_send_*()` helpers

### Transport
- **MQTT Transport**: `nrf/subsys/net/l2_dect_nrp/rpc/common/dect_rpc_mqtt_transport.c`
  - `mqtt_transport_send()` - sends via `nrf_cloud_send()`

### nRF Cloud
- **Send API**: `nrf/subsys/net/lib/nrf_cloud/src/nrf_cloud.c` - `nrf_cloud_send()`
- **MQTT Publish**: `nrf/subsys/net/lib/nrf_cloud/src/nrf_cloud_transport.c` - `nct_dc_send()`

## Testing

### Send Command from Cloud

```bash
# Publish command to c2d topic
mosquitto_pub -h mqtt.nrfcloud.com -p 8883 \
  --cafile ca.pem --cert client.pem --key client.key \
  -t "YOUR_DEVICE_ID/c2d" \
  -m "$(echo '81 1D FF 00 00 81 00' | xxd -r -p)"
```

### Receive Response from Cloud

```bash
# Subscribe to d2c topic
mosquitto_sub -h mqtt.nrfcloud.com -p 8883 \
  --cafile ca.pem --cert client.pem --key client.key \
  -t "YOUR_DEVICE_ID/d2c" \
  -v
```

### Expected Response

```
YOUR_DEVICE_ID/d2c 01 1D 01 00 00 [CBOR payload...]
```

## Notes

1. **QoS Level**: Responses use `MQTT_QOS_1_AT_LEAST_ONCE` for reliable delivery
2. **Binary Format**: All data is binary (not JSON) - both commands and responses
3. **Context ID Reuse**: The server uses the request's source context ID as the response's destination context ID
4. **Same Topic Pattern**: Uses standard nRF Cloud data channel topics (no custom topics needed)
5. **Automatic Routing**: nRF Cloud automatically routes `NRF_CLOUD_TOPIC_MESSAGE` to the `d2c` topic

