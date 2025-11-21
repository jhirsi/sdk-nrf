# Troubleshooting: UART RPC CRC Errors and Data Corruption

## Error Symptoms

```
[00:00:10.865,692] <err> nrf_rpc_uart: Invalid packet CRC: calculated 039d but received 000a
[00:00:10.916,107] <wrn> nrf_rpc_uart: Duplicate packet 1b26
[00:00:10.916,290] <wrn> nrf_rpc_uart: Ack timeout
[00:00:11.016,906] <err> NRF_RPC: Remote group does not match local group
```

## What This Means

**Good news**: Data **is** getting through the UART link (unlike before)!

**Bad news**: The data is **corrupted** during transmission, causing:
- CRC mismatches (calculated CRC doesn't match received CRC)
- Invalid group string IDs (corrupted packets can't be matched to groups)
- Duplicate packet warnings (retransmissions due to missed ACKs)
- ACK timeouts (corrupted ACKs aren't recognized)

## Root Cause: Signal Integrity Issues

At **1 Mbps baud rate**, the UART link is very sensitive to:
- **Noise and interference**
- **Poor connections**
- **Long wires**
- **Missing or improper flow control**
- **Ground loops**

## Immediate Fixes

### 1. **Verify Hardware Flow Control is Working** ⚠️ **CRITICAL**

Hardware flow control (RTS/CTS) is **mandatory** for reliable 1 Mbps communication.

**Check connections:**
- [ ] Server RTS (P0.16) → Client CTS (P0.17) ✅
- [ ] Server CTS (P0.17) → Client RTS (P0.16) ✅

**Verify in devicetree:**
Both overlay files must have:
```dts
&uart1 {
    status = "okay";
    hw-flow-control;  /* This is critical! */
    current-speed = <1000000>;
};
```

**Test flow control:**
- If RTS/CTS lines are not connected, the UART will send data even when the receiver buffer is full
- This causes data loss and corruption
- **Flow control MUST be connected for reliable operation**

### 2. **Reduce Wire Length**

**Problem**: Long wires act as antennas and pick up noise.

**Solution**:
- Use **short wires** (< 30 cm recommended for 1 Mbps)
- Keep wires **twisted together** to reduce interference
- Use **shielded cables** if wires must be longer
- Keep wires **away from power supplies** and other noise sources

### 3. **Improve Ground Connection**

**Problem**: Poor ground connection causes ground loops and noise.

**Solution**:
- Use a **thick, short ground wire**
- Connect GND **directly** (not through breadboard)
- Ensure **good contact** (no oxidation)
- Use **star grounding** if possible (all grounds meet at one point)

### 4. **Reduce Baud Rate (Temporary Test)**

If the above doesn't work, try a lower baud rate to verify the hardware works:

**In both overlay files, change:**
```dts
current-speed = <115200>;  /* Instead of 1000000 */
```

**Rebuild both client and server:**
```bash
west build -p -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_server -- -DOVERLAY_CONFIG=overlay-uart.conf
west build -p -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_client
```

If 115200 works but 1 Mbps doesn't, it's definitely a signal integrity issue.

### 5. **Check for Interference**

**Sources of interference:**
- Switching power supplies
- Motors or relays
- Other UART/SPI/I2C devices
- Radio transmitters
- Fluorescent lights

**Solution**:
- Move devices away from interference sources
- Use ferrite cores on wires
- Add decoupling capacitors near UART pins

### 6. **Verify Pin Connections**

**Double-check all connections:**
- TX→RX (crossed) ✅
- RTS→CTS (crossed) ✅
- GND→GND (straight) ✅

**Use a multimeter** to verify continuity and check for:
- Intermittent connections
- High resistance (> 1 ohm)
- Shorts between wires

### 7. **Check UART Device Configuration**

**Verify UART is configured correctly:**

Check logs for:
```
[00:00:00.xxx] <inf> uart_nrfx_uarte: UART_1 initialized
```

If you see UART errors, the device might not be configured correctly.

## Debugging Steps

### Step 1: Enable Maximum Debug Logging

Add to both `prj.conf`:
```kconfig
CONFIG_NRF_RPC_TR_LOG_LEVEL_DBG=y
CONFIG_NRF_RPC_LOG_LEVEL_DBG=y
CONFIG_UART_LOG_LEVEL_DBG=y
```

This will show:
- Packet transmission/reception
- CRC calculations
- ACK handling
- HDLC frame decoding

### Step 2: Monitor CRC Error Rate

**Good link**: No CRC errors or very occasional errors (< 1 per minute)

**Bad link**: Frequent CRC errors (> 1 per second)

If you see frequent CRC errors, the link quality is poor.

### Step 3: Check for Pattern in Errors

**Random errors**: Usually noise/interference

**Systematic errors**: Could indicate:
- Baud rate mismatch (unlikely if both configured same)
- Flow control not working
- Buffer overflow

### Step 4: Test with Different Hardware

Try:
- Different wires
- Different connectors
- Different power supplies
- Different board positions

If errors change, you've identified the source.

## Advanced Solutions

### 1. **Add Series Resistors**

Add 22-33 ohm series resistors on TX lines to reduce reflections:
```
Server TX → [22Ω] → Client RX
```

### 2. **Add Pull-up/Pull-down Resistors**

Some UART implementations benefit from weak pull-ups on RX lines:
```
Client RX → [10kΩ] → VCC
```

### 3. **Use Differential Signaling**

For very long distances, consider using RS-485 transceivers instead of direct UART.

### 4. **Reduce Baud Rate Permanently**

If 1 Mbps is not required, use a lower baud rate:
- 115200 baud: More reliable, slower
- 230400 baud: Good compromise
- 460800 baud: Faster, but still more reliable than 1 Mbps

## Expected Behavior When Working

**Server:**
```
[00:00:00.xxx] <inf> dect_rpc_server_sample: DECT RPC server ready
[00:00:00.xxx] <dbg> nrf_rpc_uart: <<< TX packet [INIT] 1234
[00:00:00.xxx] <dbg> nrf_rpc_uart: >>> RX ack 1234
[00:00:00.xxx] <dbg> nrf_rpc_uart: Acked successfully
[00:00:00.xxx] <dbg> nrf_rpc_uart: >>> RX packet [INIT] 5678
[00:00:00.xxx] <dbg> nrf_rpc_uart: <<< TX ack 5678
```

**Client:**
```
[00:00:00.xxx] <inf> dect_rpc_client_sample: DECT RPC client ready
[00:00:00.xxx] <dbg> nrf_rpc_uart: <<< TX packet [INIT] 5678
[00:00:00.xxx] <dbg> nrf_rpc_uart: >>> RX ack 5678
[00:00:00.xxx] <dbg> nrf_rpc_uart: Acked successfully
[00:00:00.xxx] <inf> dect_rpc_client_sample: DECT RPC group bound
```

**No CRC errors, no duplicate packets, no ACK timeouts!**

## Quick Checklist

- [ ] Hardware flow control (RTS/CTS) connected and working
- [ ] Wires are short (< 30 cm)
- [ ] Ground connection is good (thick wire, good contact)
- [ ] No interference sources nearby
- [ ] All 5 wires connected correctly
- [ ] UART configured with `hw-flow-control` in devicetree
- [ ] Both devices using same baud rate
- [ ] Good quality wires and connectors

## Summary

CRC errors indicate **data corruption** during transmission. This is typically caused by:

1. **Missing or broken flow control** (most common)
2. **Long wires** picking up noise
3. **Poor ground connection**
4. **Interference** from other devices
5. **Signal integrity issues** at high baud rates

**Most likely fix**: Ensure hardware flow control (RTS/CTS) is properly connected and working. This is **critical** for reliable 1 Mbps communication.

If CRC errors persist after fixing flow control, try reducing the baud rate to 115200 to verify the hardware works, then gradually increase it to find the maximum reliable speed for your setup.

