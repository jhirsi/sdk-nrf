# Troubleshooting "Error on receive reported -14, group unknown, id 0x00, type 0x04"

## Error Breakdown

**Error Code**: `-14` = `NRF_EFAULT` (Bad address)  
**Packet Type**: `0x04` = `NRF_RPC_PACKET_TYPE_INIT` (Initialization packet)  
**Group**: `unknown` (group couldn't be found)  
**ID**: `0x00` (initialization packet ID)

## What This Error Means

This error occurs when:

1. **The client sends an INIT packet** to bind/initialize the RPC group
2. **The server receives the INIT packet** but **cannot find a matching local RPC group**
3. The server looks for a group with the same string ID (`strid`) as specified in the INIT packet
4. No matching group is found, so the server returns error `-14` (NRF_EFAULT)

The error is generated in `init_packet_handle()` at line 686-689 of `nrf_rpc.c`:

```c
*group = group_from_strid(init_data.strid, init_data.strid_len);
if (*group == NULL) {
    NRF_RPC_ERR("Remote group does not match local group");
    NRF_RPC_ASSERT(false);
    return -NRF_EFAULT;  // This is error -14
}
```

## Common Causes

### 1. **Server Not Initialized** ⚠️ **MOST COMMON**

**Symptom**: Server hasn't initialized the DECT RPC group yet.

**Solution**: 
- Ensure `dect_rpc_server_init()` is called on the server
- Check server logs for initialization messages:
  ```
  [00:00:00.xxx] <inf> dect_rpc_server_sample: DECT RPC server ready
  ```

**Verification**: The server must call:
```c
nrf_rpc_init(err_handler);
dect_rpc_server_init();  // This registers the "dect_rpc" group
```

### 2. **Group String ID Mismatch**

**Symptom**: Client and server have different group string IDs.

**Solution**: 
- Both must use the same group string ID: `"dect_rpc"`
- Check `dect_rpc_group.c` - it should define:
  ```c
  NRF_RPC_GROUP_DEFINE(dect_rpc_group, "dect_rpc", ...);
  ```

**Verification**: The group string ID must match exactly (case-sensitive).

### 3. **Server RPC Not Enabled**

**Symptom**: Server doesn't have DECT RPC server enabled in Kconfig.

**Solution**: 
- Verify `prj.conf` has:
  ```kconfig
  CONFIG_DECT_NRP_RPC=y
  CONFIG_DECT_NRP_RPC_SERVER=y
  ```

### 4. **Initialization Order Issue**

**Symptom**: Client tries to connect before server is ready.

**Solution**: 
- **Start the server first**, wait for it to initialize
- Then start the client
- The server must be ready to receive INIT packets

### 5. **Transport Not Initialized**

**Symptom**: UART transport not working or not initialized.

**Solution**: 
- Check hardware connections (see TROUBLESHOOTING_UART_RPC.md)
- Verify UART transport is configured:
  ```kconfig
  CONFIG_NRF_RPC_UART_TRANSPORT=y
  ```

## Debugging Steps

### Step 1: Verify Server Initialization

Check server logs for:
```
[00:00:00.xxx] <inf> dect_rpc_server_sample: Initializing DECT RPC server
[00:00:00.xxx] <inf> dect_rpc_server_sample: DECT RPC server ready
```

If these messages are missing, the server hasn't initialized the RPC group.

### Step 2: Enable Debug Logging

Add to server `prj.conf`:
```kconfig
CONFIG_NRF_RPC_LOG_LEVEL_DBG=y
CONFIG_NRF_RPC_TR_LOG_LEVEL_DBG=y
```

This will show:
- Group registration
- INIT packet reception
- Group matching attempts

### Step 3: Check Group String ID

Verify both client and server use the same group string ID. In `dect_rpc_group.c`:
```c
NRF_RPC_GROUP_DEFINE(dect_rpc_group, "dect_rpc", ...);
```

The string `"dect_rpc"` must match exactly.

### Step 4: Verify Transport Connection

The INIT packet must be received over the transport. Check:
- UART connections are correct
- Both devices are powered
- Transport is initialized on both sides

### Step 5: Check Initialization Sequence

**Correct sequence:**
1. Server: `nrf_rpc_init()` → `dect_rpc_server_init()`
2. Wait for server to be ready
3. Client: `nrf_rpc_init()` → client starts sending INIT packet
4. Server receives INIT and matches group

## Expected Behavior

**When working correctly:**

1. **Client sends INIT packet**:
   ```
   [00:00:00.xxx] <dbg> nrf_rpc: Sending INIT packet for group "dect_rpc"
   ```

2. **Server receives and matches group**:
   ```
   [00:00:00.xxx] <dbg> nrf_rpc: Received INIT packet, group "dect_rpc"
   [00:00:00.xxx] <dbg> nrf_rpc: Found corresponding local group
   [00:00:00.xxx] <dbg> nrf_rpc: Group bound successfully
   ```

3. **No error messages**

## Quick Checklist

- [ ] Server has `CONFIG_DECT_NRP_RPC_SERVER=y`
- [ ] Server calls `dect_rpc_server_init()`
- [ ] Server logs show "DECT RPC server ready"
- [ ] Client and server use same group string ID ("dect_rpc")
- [ ] UART transport is working (no "Ack timeout" errors)
- [ ] Server initialized before client tries to connect
- [ ] Both devices powered and connected

## Related Errors

- **"Ack timeout"**: Transport layer issue (see TROUBLESHOOTING_UART_RPC.md)
- **"Invalid group id"**: Different error - group ID mismatch in packet header
- **"Remote group does not match local group"**: Same root cause, different log message

## Still Having Issues?

1. **Enable maximum debug logging**:
   ```kconfig
   CONFIG_NRF_RPC_LOG_LEVEL_DBG=y
   CONFIG_NRF_RPC_TR_LOG_LEVEL_DBG=y
   CONFIG_DECT_NRP_RPC_LOG_LEVEL_DBG=y
   ```

2. **Check for other error messages** that might indicate the root cause

3. **Verify both samples are from the same codebase** - group string IDs must match

4. **Try rebuilding both samples** to ensure latest code is used

