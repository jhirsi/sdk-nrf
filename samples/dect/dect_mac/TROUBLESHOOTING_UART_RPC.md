# Troubleshooting "nrf_rpc_uart: Ack timeout" Error

## Error Description

The "nrf_rpc_uart: Ack timeout" error occurs when the server (or client) sends a packet over UART but does not receive an acknowledgment (ACK) from the remote device within the configured timeout period.

This error is logged at line 455 in `nrf_rpc_uart.c` when:
```c
k_sem_take(&uart_tr->ack_sem, K_MSEC(CONFIG_NRF_RPC_UART_ACK_WAITING_TIME))
```
times out.

## Common Causes and Solutions

### 1. **Client Not Running or Not Initialized** ⚠️ **MOST COMMON**

**Symptom**: Server starts but client hasn't been flashed or initialized yet.

**Solution**:
- Ensure the client device is powered on
- Flash the client sample: `west flash`
- Wait for the client to fully initialize (check client logs)
- The client must call `nrf_rpc_init()` before the server can communicate with it

**Verification**: Check client terminal for initialization messages:
```
[00:00:00.xxx] <inf> dect_rpc_client_sample: DECT RPC client ready
```

### 2. **Hardware Connection Issues**

**Symptom**: Wires not connected correctly or loose connections.

**Solution**: Verify all connections according to the README:

**For nRF9151DK:**
- Server UART1 TX (P0.29) → Client UART1 RX (P0.28)
- Server UART1 RX (P0.28) → Client UART1 TX (P0.29)
- Server UART1 RTS (P0.16) → Client UART1 CTS (P0.17)
- Server UART1 CTS (P0.17) → Client UART1 RTS (P0.16)
- GND → GND (both devices)

**Important**: 
- **TX must connect to RX** (crossed)
- **RTS must connect to CTS** (crossed)
- **GND must be connected** (common ground is essential)

### 3. **Flow Control (RTS/CTS) Not Connected**

**Symptom**: Data lines work but flow control fails.

**Solution**: 
- Both samples have `CONFIG_NRF_RPC_UART_RELIABLE=y` which requires hardware flow control
- **RTS/CTS lines are mandatory** - the connection will not work without them
- Verify RTS/CTS connections are correct and not swapped

### 4. **Baud Rate Mismatch**

**Symptom**: Devices configured with different baud rates.

**Solution**: 
- Both devices must use the same baud rate
- Default is 1 Mbps (1000000) as configured in the overlay files
- Verify both `boards/nrf9151dk_nrf9151_ns.overlay` files have:
  ```dts
  current-speed = <1000000>;
  ```

### 5. **Wrong UART Port**

**Symptom**: Using wrong UART (e.g., UART0 instead of UART1).

**Solution**:
- Verify both overlays specify `nordic,rpc-uart = &uart1;`
- UART1 is used for RPC, UART0 is typically used for console/shell

### 6. **Initialization Order**

**Symptom**: Server tries to communicate before client is ready.

**Solution**:
- **Start the client first**, then start the server
- Or wait a few seconds after starting the server before it tries to communicate
- The server will retry (up to `CONFIG_NRF_RPC_UART_TX_ATTEMPTS` times)

### 7. **Configuration Mismatch**

**Symptom**: Client and server have different RPC configurations.

**Solution**: Verify both have:
- `CONFIG_NRF_RPC_UART_TRANSPORT=y`
- `CONFIG_NRF_RPC_UART_RELIABLE=y` (both must match)
- `CONFIG_UART_LINE_CTRL=y`
- `CONFIG_UART_INTERRUPT_DRIVEN=y`

## Debugging Steps

### Step 1: Verify Hardware Connections

1. **Power off both devices**
2. **Check connections** with a multimeter (continuity test)
3. **Verify pin assignments** match the board definition
4. **Ensure GND is connected** (critical!)

### Step 2: Check Device Initialization

**Server logs should show:**
```
[00:00:00.xxx] <inf> dect_rpc_server_sample: DECT RPC server ready
[00:00:00.xxx] <inf> dect_rpc_server_sample: Waiting for RPC commands from client...
```

**Client logs should show:**
```
[00:00:00.xxx] <inf> dect_rpc_client_sample: DECT RPC client ready
[00:00:00.xxx] <inf> dect_rpc_client_sample: Use shell commands to control DECT stack
```

### Step 3: Enable Debug Logging

Add to both `prj.conf` files:
```kconfig
CONFIG_NRF_RPC_TR_LOG_LEVEL_DBG=y
CONFIG_NRF_RPC_LOG_LEVEL_DBG=y
```

This will show detailed UART transport logs including:
- TX/RX packet information
- ACK send/receive
- Timeout details

### Step 4: Test with Simple Command

On the client, try a simple command:
```bash
uart:~$ dect status
```

If this works, the connection is established. If it times out, check the hardware connections.

### Step 5: Check UART Configuration

Verify both overlay files are identical:
```dts
&uart1 {
    status = "okay";
    hw-flow-control;
    current-speed = <1000000>;
};
```

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

- [ ] Both devices powered on
- [ ] Client sample flashed and running
- [ ] Server sample flashed and running
- [ ] TX→RX connections (crossed)
- [ ] RX→TX connections (crossed)
- [ ] RTS→CTS connections (crossed)
- [ ] CTS→RTS connections (crossed)
- [ ] GND connected between devices
- [ ] Same baud rate on both devices (1 Mbps)
- [ ] Both using UART1 (not UART0)
- [ ] Both have `CONFIG_NRF_RPC_UART_RELIABLE=y`
- [ ] Client initialized before server tries to communicate

## Still Having Issues?

1. **Try disabling reliable mode** (for testing only):
   ```kconfig
   CONFIG_NRF_RPC_UART_RELIABLE=n
   ```
   This disables ACK requirement but is less reliable.

2. **Check for interference**: Keep UART wires short and away from power lines

3. **Verify board compatibility**: Ensure both are nRF9151DK boards

4. **Check terminal output**: Look for other error messages that might indicate the root cause

