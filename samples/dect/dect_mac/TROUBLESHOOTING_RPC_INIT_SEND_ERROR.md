# Troubleshooting "Failed to send group init packet for group id: 0 strid: dect_rpc err: -71"

## Error Breakdown

**Error Code**: `-71` = `EPROTO` (Protocol error)  
**Packet Type**: INIT packet (group initialization)  
**Group**: `dect_rpc`  
**Group ID**: `0` (not yet assigned - initialization phase)

## What This Error Means

This error occurs when:

1. **The server tries to send an INIT packet** to initialize/bind the RPC group
2. **The server is configured as the initiator** (`NRF_RPC_FLAGS_INITIATOR` flag)
3. **The INIT packet is sent over UART** but **no ACK is received from the client**
4. **After all retry attempts fail**, the transport returns `-EPROTO` (-71)

The error is generated in `nrf_rpc_uart.c` at line 464:
```c
return acked ? 0 : -EPROTO;  // Returns -71 if ACK not received
```

This happens in the `send()` function when `CONFIG_NRF_RPC_UART_RELIABLE=y` is enabled and the ACK timeout occurs after all retry attempts.

## Root Cause

**The client is not responding to the server's INIT packet.** This is the same underlying issue as "Ack timeout" - the UART communication is failing.

## Common Causes

### 1. **Client Not Running or Not Initialized** ⚠️ **MOST COMMON**

**Symptom**: Server sends INIT but client hasn't started yet.

**Solution**:
- Ensure the client device is powered on
- Flash the client sample: `west flash`
- Wait for the client to fully initialize
- The client must be ready to receive and acknowledge packets

**Verification**: Check client terminal for:
```
[00:00:00.xxx] <inf> dect_rpc_client_sample: DECT RPC client ready
```

### 2. **Hardware Connection Issues**

**Symptom**: UART wires not connected correctly.

**Solution**: Verify all connections:
- Server UART1 TX (P0.29) → Client UART1 RX (P0.28)
- Server UART1 RX (P0.28) → Client UART1 TX (P0.29)
- Server UART1 RTS (P0.16) → Client UART1 CTS (P0.17)
- Server UART1 CTS (P0.17) → Client UART1 RTS (P0.16)
- GND → GND

**Important**: All 5 connections are required, especially RTS/CTS for reliable mode.

### 3. **Flow Control (RTS/CTS) Not Working**

**Symptom**: Data lines work but flow control fails.

**Solution**: 
- With `CONFIG_NRF_RPC_UART_RELIABLE=y`, RTS/CTS are **mandatory**
- Verify RTS/CTS connections are correct and not swapped
- Check that hardware flow control is enabled in devicetree: `hw-flow-control;`

### 4. **Initialization Order Issue**

**Symptom**: Server tries to send INIT before client is ready.

**Solution**: 
- **Start the client first**, wait for it to initialize
- Then start the server
- Or wait a few seconds after starting the server

### 5. **Baud Rate Mismatch**

**Symptom**: Devices configured with different baud rates.

**Solution**: 
- Both devices must use the same baud rate (default: 1 Mbps)
- Verify both overlay files have: `current-speed = <1000000>;`

### 6. **UART Not Initialized**

**Symptom**: UART transport not properly initialized.

**Solution**:
- Check that `nrf_rpc_init()` is called before `dect_rpc_server_init()`
- Verify UART device is configured in devicetree
- Check that `nordic,rpc-uart = &uart1;` is set in the overlay

## Debugging Steps

### Step 1: Verify Client is Running

**Client logs should show:**
```
[00:00:00.xxx] <inf> dect_rpc_client_sample: DECT RPC client ready
[00:00:00.xxx] <inf> dect_rpc_client_sample: Use shell commands to control DECT stack
```

If these are missing, the client hasn't initialized.

### Step 2: Check Hardware Connections

1. **Power off both devices**
2. **Verify all 5 connections** with a multimeter
3. **Check pin assignments** match the board definition
4. **Ensure GND is connected** (critical!)

### Step 3: Enable Debug Logging

Add to both `prj.conf` files:
```kconfig
CONFIG_NRF_RPC_TR_LOG_LEVEL_DBG=y
CONFIG_NRF_RPC_LOG_LEVEL_DBG=y
```

This will show:
- INIT packet transmission attempts
- ACK reception status
- Retry attempts
- Timeout details

### Step 4: Check UART Configuration

Verify both overlay files are identical:
```dts
&uart1 {
    status = "okay";
    hw-flow-control;
    current-speed = <1000000>;
};
```

### Step 5: Verify Initialization Sequence

**Server should:**
1. Call `nrf_rpc_init(err_handler)`
2. Call `dect_rpc_server_init()`
3. Wait for transport to initialize
4. Try to send INIT packet (as initiator)

**Client should:**
1. Call `nrf_rpc_init(err_handler)`
2. Be ready to receive INIT packet
3. Send ACK when INIT is received

## Expected Behavior

**When working correctly:**

1. **Server sends INIT packet**:
   ```
   [00:00:00.xxx] <dbg> nrf_rpc_uart: <<< TX packet [INIT data]
   ```

2. **Client receives and acknowledges**:
   ```
   [00:00:00.xxx] <dbg> nrf_rpc_uart: >>> RX ack [CRC]
   ```

3. **Server receives ACK**:
   ```
   [00:00:00.xxx] <dbg> nrf_rpc_uart: Acked successfully
   [00:00:00.xxx] <dbg> nrf_rpc: Group bound successfully
   ```

4. **No error messages**

## Configuration Options

You can adjust timeout and retry behavior in `prj.conf`:

```kconfig
# ACK waiting timeout (default: 50ms)
CONFIG_NRF_RPC_UART_ACK_WAITING_TIME=50

# Maximum transmission attempts (default: 3)
CONFIG_NRF_RPC_UART_TX_ATTEMPTS=3
```

**Note**: Increasing timeout may help if you have slow hardware, but it's better to fix the root cause.

## Quick Checklist

- [ ] Client device powered on
- [ ] Client sample flashed and running
- [ ] Server sample flashed and running
- [ ] All 5 UART wires connected correctly
- [ ] RTS/CTS flow control connected (mandatory for reliable mode)
- [ ] GND connected between devices
- [ ] Same baud rate on both devices (1 Mbps)
- [ ] Both using UART1
- [ ] Client initialized before server tries to send INIT
- [ ] Hardware flow control enabled in devicetree

## Relationship to Other Errors

- **"Ack timeout"**: Same root cause - client not responding
- **"Error on receive -14"**: Different issue - server can't find matching group
- **Error -71**: This error - server can't send INIT packet (no ACK from client)

All three errors typically indicate the same problem: **UART communication is not working** between client and server.

## Still Having Issues?

1. **Try disabling reliable mode** (for testing only):
   ```kconfig
   CONFIG_NRF_RPC_UART_RELIABLE=n
   ```
   This disables ACK requirement but is less reliable.

2. **Check for other error messages** that might indicate the root cause

3. **Verify both devices are nRF9151DK** and using the same firmware versions

4. **Test UART independently** - try sending simple data to verify hardware works

