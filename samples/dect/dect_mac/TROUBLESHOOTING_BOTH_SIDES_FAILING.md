# Troubleshooting: Both Client and Server Failing to Send INIT Packets

## Error Symptoms

**Server side:**
```
Failed to send group init packet for group id: 0 strid: dect_rpc err: -71
```

**Client side:**
```
Failed to send group init packet for group id: 0 strid: dect_rpc err: -71
Ack timeout (multiple times)
Failed to send group init packet for group id: 1 strid: rpc_utils err: -71
```

## What This Means

**Both devices are trying to send INIT packets but neither is receiving ACKs.** This indicates:

1. **The UART link is completely broken** - no data is getting through in either direction
2. **Both sides are configured as initiators** (which is fine - both can be initiators)
3. **Hardware connection issue** - most likely cause

## Root Cause Analysis

When **both** client and server fail simultaneously, it means:

- ❌ **No UART data is being transmitted** (or received)
- ❌ **Hardware connection is broken** or misconfigured
- ❌ **UART device is not working** on one or both devices

This is **NOT** a software configuration issue - it's a **hardware/connection problem**.

## Immediate Checks

### 1. **Verify Hardware Connections** ⚠️ **CRITICAL**

**Check all 5 connections with a multimeter (continuity test):**

- [ ] Server UART1 TX (P0.29) → Client UART1 RX (P0.28)
- [ ] Server UART1 RX (P0.28) → Client UART1 TX (P0.29)  
- [ ] Server UART1 RTS (P0.16) → Client UART1 CTS (P0.17)
- [ ] Server UART1 CTS (P0.17) → Client UART1 RTS (P0.16)
- [ ] GND → GND (both devices)

**Common mistakes:**
- TX connected to TX (should be TX→RX)
- RX connected to RX (should be RX→TX)
- RTS connected to RTS (should be RTS→CTS)
- Missing GND connection
- Loose connections

### 2. **Check UART Device Status**

Verify UART1 is enabled and configured:

**On both devices, check logs for:**
```
[00:00:00.xxx] <inf> uart_nrfx_uarte: UART_1 initialized
```

If you see UART errors, the device might not be configured correctly.

### 3. **Verify Devicetree Configuration**

Both overlay files should have:
```dts
&uart1 {
    status = "okay";
    hw-flow-control;
    current-speed = <1000000>;
};
```

### 4. **Test UART Independently**

Try a simple UART test to verify hardware works:

1. **Disconnect RPC connection**
2. **Use a simple UART echo test** on both devices
3. **Verify data can be sent/received**

If simple UART doesn't work, the hardware connection is definitely broken.

## Debugging Steps

### Step 1: Enable Maximum Debug Logging

Add to both `prj.conf` files:
```kconfig
CONFIG_NRF_RPC_TR_LOG_LEVEL_DBG=y
CONFIG_NRF_RPC_LOG_LEVEL_DBG=y
CONFIG_UART_LOG_LEVEL_DBG=y
```

This will show:
- UART initialization
- Packet transmission attempts
- ACK waiting/timeout
- UART errors

### Step 2: Check for UART Errors

Look for these error messages:
- `UART error: ...`
- `Failed to initialize UART`
- `UART device not found`

### Step 3: Verify Pin Assignments

Double-check that you're using the correct pins:
- **P0.29** = UART1 TX
- **P0.28** = UART1 RX
- **P0.16** = UART1 RTS
- **P0.17** = UART1 CTS

**Note**: These pins may not be directly accessible on the Arduino header - check the Hardware User Guide for test points or alternative access methods.

### Step 4: Test with Different Baud Rate

Try a lower baud rate to see if it's a signal integrity issue:

In both overlay files, change:
```dts
current-speed = <115200>;  /* Instead of 1000000 */
```

If this works, you may have signal integrity issues at 1 Mbps.

### Step 5: Check Power Supply

- Ensure both devices are properly powered
- Check for power supply noise
- Try different power sources

### Step 6: Verify Both Devices are nRF9151DK

- Ensure both are the same board type
- Check firmware versions match
- Verify both are using the same UART configuration

## Common Hardware Issues

### 1. **Wires Too Long**

**Symptom**: Works at low baud rates but fails at high rates.

**Solution**: Use shorter wires (< 30 cm recommended for 1 Mbps)

### 2. **Poor Connections**

**Symptom**: Intermittent failures.

**Solution**: 
- Use proper connectors (not breadboard)
- Ensure good contact
- Check for loose wires

### 3. **Missing GND**

**Symptom**: No communication at all.

**Solution**: **GND connection is mandatory** - verify it's connected

### 4. **Wrong Pin Assignments**

**Symptom**: No communication.

**Solution**: Double-check pin assignments match the board definition

### 5. **UART Device Not Enabled**

**Symptom**: UART errors in logs.

**Solution**: Verify `status = "okay";` in devicetree overlay

## Quick Test Procedure

1. **Power off both devices**
2. **Disconnect all wires**
3. **Check each wire individually** with multimeter:
   - Continuity from one end to the other
   - No shorts between wires
4. **Reconnect carefully**:
   - TX→RX (crossed)
   - RTS→CTS (crossed)
   - GND→GND (straight)
5. **Power on both devices**
6. **Check logs** - should see initialization, not errors

## Expected Behavior When Working

**Server:**
```
[00:00:00.xxx] <inf> dect_rpc_server_sample: Initializing RPC server
[00:00:00.xxx] <inf> dect_rpc_server_sample: Initializing DECT RPC server
[00:00:00.xxx] <dbg> nrf_rpc_uart: <<< TX packet [INIT]
[00:00:00.xxx] <dbg> nrf_rpc_uart: Acked successfully
[00:00:00.xxx] <inf> dect_rpc_server_sample: DECT RPC server ready
```

**Client:**
```
[00:00:00.xxx] <inf> dect_rpc_client_sample: Initializing RPC client
[00:00:00.xxx] <dbg> nrf_rpc_uart: <<< TX packet [INIT]
[00:00:00.xxx] <dbg> nrf_rpc_uart: Acked successfully
[00:00:00.xxx] <inf> dect_rpc_client_sample: DECT RPC client ready
```

**No error messages!**

## Still Not Working?

1. **Try disabling reliable mode** (temporary test):
   ```kconfig
   CONFIG_NRF_RPC_UART_RELIABLE=n
   ```
   This removes ACK requirement but is less reliable.

2. **Use a different UART** (if available) to test if UART1 hardware is faulty

3. **Check for physical damage** to the boards or connectors

4. **Try different cables/wires** to rule out faulty hardware

5. **Test with a known-working UART setup** (e.g., UART0 console) to verify basic UART functionality

## Summary

When **both** client and server fail simultaneously, this is **definitely a hardware connection issue**. The software is working correctly - the problem is that no data is getting through the UART link.

**Focus on:**
1. ✅ Hardware connections (all 5 wires)
2. ✅ Pin assignments (correct pins)
3. ✅ GND connection (mandatory)
4. ✅ Wire quality and length
5. ✅ UART device configuration

