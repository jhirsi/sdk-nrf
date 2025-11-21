# DECT RPC MQTT Transport Architecture Review

## Overview

This document reviews the architecture for implementing nRF Cloud MQTT as a transport layer for DECT NR+ RPC.

## Current State

### nRF RPC Transport Interface

nRF RPC requires implementing the `nrf_rpc_tr_api` interface:

```c
struct nrf_rpc_tr_api {
    int (*init)(const struct nrf_rpc_tr *transport, 
                nrf_rpc_tr_receive_handler_t receive_cb, void *context);
    int (*send)(const struct nrf_rpc_tr *transport, 
                const uint8_t *data, size_t length);
    void *(*tx_buf_alloc)(const struct nrf_rpc_tr *transport, size_t *size);
    void (*tx_buf_free)(const struct nrf_rpc_tr *transport, void *buf);
    void (*rx_buf_free)(const struct nrf_rpc_tr *transport, void *buf); // optional
};
```

### nRF Cloud MQTT Architecture

- **MQTT Client**: Stored as static `nct.client` in `nrf_cloud_transport.c`
- **Event Handler**: `nct_mqtt_evt_handler()` processes all MQTT events
- **Data Channel**: Uses `nct_dc_send()` for sending, subscribes to device-specific topics
- **Topic Structure**: `<device_id>/<endpoint>/<direction>`
  - Example: `<device_id>/d2c` (device-to-cloud TX)
  - Example: `<device_id>/c2d` (cloud-to-device RX)

## Architecture Options

### Option 1: Direct MQTT Client Access (Recommended)

**Approach**: Get MQTT client reference and subscribe directly to custom topics.

**Pros**:
- Full control over topic subscription
- Direct access to MQTT API
- Similar to FOTA implementation pattern

**Cons**:
- Need to access MQTT client (currently static)
- Need to hook into MQTT event handler or duplicate logic

**Implementation**:
1. Add function to nRF Cloud to get MQTT client (or use internal access)
2. Subscribe to custom topic: `<device_id>/dect/rpc/cmd`
3. Publish to custom topic: `<device_id>/dect/rpc/rsp` and `<device_id>/dect/rpc/evt`
4. Intercept MQTT events for our topics

### Option 2: Use nRF Cloud Data Channel with Custom Topics

**Approach**: Use `nct_dc_send()` but with custom topic configuration.

**Pros**:
- Uses existing nRF Cloud infrastructure
- Handles connection state automatically

**Cons**:
- Less control over topic structure
- May conflict with existing data channel usage
- Topic routing might be complex

### Option 3: Hook into nRF Cloud Event System

**Approach**: Register custom handler for specific topics via nRF Cloud events.

**Pros**:
- Clean integration with nRF Cloud
- Uses existing event infrastructure

**Cons**:
- Requires nRF Cloud API changes
- Less direct control

## Recommended Architecture (Option 1 with Modifications)

### Design Decisions

1. **MQTT Client Access**: 
   - Create internal function to get MQTT client (similar to FOTA pattern)
   - Or: Store client reference during initialization callback

2. **Topic Structure**:
   ```
   RX (subscribe): <device_id>/dect/rpc/cmd    # Commands from cloud
   TX (publish):   <device_id>/dect/rpc/rsp    # Responses to cloud
   TX (publish):   <device_id>/dect/rpc/evt    # Events to cloud
   ```

3. **Initialization Flow**:
   ```
   1. nrf_cloud_init()
   2. nrf_cloud_connect() → Wait for MQTT connection
   3. nrf_rpc_init() → Calls transport init()
   4. Transport init() → Subscribe to DECT RPC topic
   5. dect_rpc_server_init()
   ```

4. **Message Flow**:
   ```
   Cloud → MQTT → nct_mqtt_evt_handler() → Check topic → 
   Route to dect_rpc_mqtt_transport → nrf_rpc receive handler
   
   Device → nrf_rpc send() → dect_rpc_mqtt_transport send() → 
   mqtt_publish() → Cloud
   ```

### Key Implementation Details

1. **MQTT Event Interception**:
   - Option A: Modify `nct_mqtt_evt_handler()` to check for DECT RPC topics
   - Option B: Register custom MQTT event callback (if supported)
   - Option C: Use nRF Cloud event system with custom topic decoder

2. **Connection State Management**:
   - Transport init can be called before MQTT connection
   - Defer subscription until MQTT is connected
   - Use nRF Cloud connection events to trigger subscription

3. **Packet Format**:
   - MQTT payload = nRF RPC packet (5-byte header + CBOR payload)
   - No HDLC framing needed (MQTT provides reliability)
   - Use MQTT QoS 1 for commands/responses

4. **Error Handling**:
   - Handle MQTT disconnection gracefully
   - Retry subscription on reconnection
   - Report errors to nRF RPC error handler

## Implementation Plan

### Phase 1: Basic Transport (MVP)

1. Create `dect_rpc_mqtt_transport.c` with transport API implementation
2. Add MQTT client access function (or use callback registration)
3. Implement topic subscription after MQTT connection
4. Route MQTT messages to nRF RPC receive handler
5. Implement send function using `mqtt_publish()`

### Phase 2: Integration

1. Add Kconfig option for MQTT transport
2. Update `dect_rpc_group.c` to support MQTT transport
3. Add initialization sequence documentation
4. Test with nRF Cloud backend

### Phase 3: Optimization

1. Add connection state management
2. Handle reconnection scenarios
3. Add error recovery
4. Performance optimization

## Considerations

### Advantages

✅ **Direct Cloud Integration**: No gateway needed
✅ **Secure**: Uses nRF Cloud TLS/MTLS
✅ **Scalable**: Supports many devices
✅ **Standard Protocol**: MQTT is widely supported
✅ **Reliable**: MQTT QoS 1 provides delivery guarantees

### Challenges

⚠️ **MQTT Client Access**: Need to access internal MQTT client
⚠️ **Event Routing**: Need to intercept MQTT events for custom topics
⚠️ **Initialization Order**: Transport init before MQTT connection
⚠️ **Topic Management**: Need to construct device-specific topics

### Risks

- **nRF Cloud API Changes**: Internal MQTT client access might break
- **Topic Conflicts**: Ensure DECT RPC topics don't conflict with other services
- **Performance**: MQTT overhead vs UART (acceptable for cloud use case)

## Conclusion

**Feasibility**: ✅ **Highly Feasible**

The architecture is sound and follows existing patterns (FOTA implementation). The main challenge is accessing the MQTT client and routing events, which can be solved with:
1. Internal function to get MQTT client (or callback registration)
2. Topic-based routing in MQTT event handler
3. Proper initialization sequence

**Recommendation**: Proceed with Option 1 (Direct MQTT Client Access) with proper abstraction to handle connection state and event routing.

