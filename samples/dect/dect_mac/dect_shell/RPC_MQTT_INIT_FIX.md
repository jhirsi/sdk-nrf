# Fix: nrf_rpc_init() Blocking with MQTT Transport

## Problem

When using DECT RPC with nRF Cloud MQTT transport in `dect_shell`, `nrf_rpc_init()` was hanging and not returning.

## Root Cause

1. **Default Group Definition**: The DECT RPC group was defined using `NRF_RPC_GROUP_DEFINE()`, which sets the `NRF_RPC_FLAGS_WAIT_ON_INIT` flag.

2. **Blocking Behavior**: `nrf_rpc_init()` waits for all groups with `WAIT_ON_INIT` flag to complete initialization by receiving an INIT packet from the remote peer.

3. **MQTT Transport Limitation**: With MQTT transport, there's no direct peer connection. The client is in the cloud and may not be connected when the server initializes. There's no INIT packet to receive immediately.

4. **Infinite Wait**: With `CONFIG_NRF_RPC_GROUP_INIT_WAIT_TIME=-1`, the wait is infinite, causing `nrf_rpc_init()` to hang forever.

## Solution

Use `NRF_RPC_GROUP_DEFINE_NOWAIT()` for MQTT transport instead of `NRF_RPC_GROUP_DEFINE()`.

### Changes Made

1. **`dect_rpc_group.c`**: Changed group definition to use `NOWAIT` when MQTT transport is enabled:
   ```c
   #if IS_ENABLED(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
   NRF_RPC_GROUP_DEFINE_NOWAIT(dect_rpc_group, "dect_rpc", &dect_rpc_tr,
                               NULL, /* ack_handler */
                               NULL, /* ack_data */
                               NULL, /* err_handler */
                               NULL, /* bound_handler */
                               false /* initiator - false means follower */);
   #else
   NRF_RPC_GROUP_DEFINE(dect_rpc_group, "dect_rpc", &dect_rpc_tr, NULL, NULL, NULL);
   #endif
   ```

2. **`overlay-nrf_cloud_mqtt_rpc.conf`**: Changed timeout from `-1` to `5000` ms (though it won't be used for NOWAIT groups):
   ```kconfig
   CONFIG_NRF_RPC_GROUP_INIT_WAIT_TIME=5000
   ```

## How NOWAIT Works

- **NOWAIT groups** don't block `nrf_rpc_init()` - it returns immediately
- The group can still receive and process commands/events
- When a client connects and sends commands, the group will handle them normally
- No INIT packet exchange is required for NOWAIT groups

## Testing

After this fix:
1. `nrf_rpc_init()` should return immediately (not hang)
2. `dect_rpc_server_init()` should complete successfully
3. RPC commands from cloud clients should work normally
4. The server can accept commands even if no client is connected yet

## Notes

- For UART/IPC transports, the original `NRF_RPC_GROUP_DEFINE()` is still used (blocking behavior is appropriate for direct peer connections)
- The `initiator=false` parameter means the server is a "follower" - it waits for clients to initiate communication
- This is appropriate for a server that responds to client commands

