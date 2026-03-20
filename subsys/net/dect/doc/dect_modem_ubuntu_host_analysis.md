# Using DECT NR+ Stack as a Modem from Ubuntu (e.g. WSL on Windows PC)

## Goal

Allow **Ubuntu** (native or under WSL2 on a Windows PC) to use the **DECT NR+ stack as a modem**: the host gets a network interface (e.g. TUN) that carries IPv6 traffic over DECT, and can run DECT control commands (join network, status, etc.) as if the modem were locally attached.

## Current Architecture (from dect_shell and DECT RPC)

- **dect_shell**: Zephyr application on **nRF9151 DK** with the DECT modem. It runs the full DECT NR+ stack and exposes a shell (`dect activate`, `dect nw_join`, `dect status`, etc.). The DECT interface is a Zephyr `net_if` (e.g. `dect0`).

- **DECT RPC** (see `nrf/subsys/net/dect/rpc/`):
  - **Server**: Runs on **nRF9151** with the DECT modem and full `l2_dect` stack. It receives raw IPv6 from the client via RPC and injects it into the DECT L2 send path; it forwards DECT receive traffic to the client via RPC.
  - **Client**: Runs on **nRF9151** (no DECT modem on that image). It exposes a Zephyr network interface `dect_rpc` that tunnels all IPv6 traffic over **UART** to the server using **nRF RPC** (CBOR over HDLC).

So today the **client is always a Zephyr device**. There is **no host-side (Linux/Windows) RPC client** in the tree. To use DECT as a modem from Ubuntu, the host must implement the RPC client role over the same UART that would otherwise go to a second board.

## Recommended Approach: Host-Side DECT RPC Client

### Concept

- **Hardware**: One **nRF9151 DK** running **dect_rpc_server** (or dect_shell with RPC server) firmware. Connect the DK to the PC via **USB** (J-Link CDC ACM). The PC sees a serial port (e.g. `/dev/ttyACM0` on Linux, `COMx` on Windows; in WSL2, USB can be forwarded so the port appears in WSL).

- **Software on Ubuntu**: A **host-side DECT RPC client** that:
  1. Opens the serial port and speaks **nRF RPC over UART** (same protocol as the Zephyr client).
  2. Acts as the **follower** in RPC init (waits for the server to send the group-init packet; server is the initiator).
  3. Implements the same commands and events as the Zephyr client (see below).
  4. Creates a **TUN interface** and bridges:
     - **TUN → device**: Read IPv6 packets from the TUN device, encode as `DECT_RPC_CMD_IF_SEND` (CBOR byte string), send over RPC.
     - **Device → TUN**: On `DECT_RPC_CMD_IF_RECEIVE` events, decode the byte string and write the packet to the TUN device.
  5. Syncs addresses/prefixes and link state from the server (GET_ADDRS, IF_LINK_STATE, IF_ADDRS_CHANGED) and configures the TUN interface accordingly.
  6. Optionally provides a CLI or small daemon to run DECT L2 commands over RPC (`DECT_RPC_CMD_SHELL`: e.g. `dect run nw_join`, `dect run status`).

Result: Ubuntu gets a TUN interface that carries IPv6 over DECT. The user can run `dect run nw_join` (or a wrapper) to join a network, and use the TUN for normal IPv6 traffic.

### RPC Protocol Summary (what the host must implement)

- **Transport**: nRF RPC UART transport.
  - **UART**: 1 Mbps (1,000,000 baud). Flow control: RTS/CTS used in the current samples (see `dect_rpc_client` / `dect_rpc_server` overlays).
  - **Framing**: HDLC-like.
    - Delimiter: `0x7e`.
    - Escape: `0x7d`; next byte is XORed with `0x20`.
    - Payload: raw bytes (RPC packet). Last 2 bytes: **CRC-16-CCITT** (seed `0xffff`), little-endian.
  - **Reliable mode** (used by samples): After receiving a valid frame, receiver sends an **ACK frame**: `0x7e` + 2 bytes (same CRC value, with flip bit for duplicate detection) + `0x7e`. Sender waits for ACK before considering the packet sent. See `nrf/subsys/nrf_rpc/nrf_rpc_uart.c` (e.g. `ack_tx`, `ack_rx`, `tx_flip`, `rx_flip_check`).

- **RPC packet payload**: Group name + CBOR-encoded message. The DECT RPC group name is `"dect_rpc"`. The exact wire format (how group id and CBOR are laid out) is defined in the nRF RPC subsys (`nrf_rpc_serialize.c`, `nrf_rpc_cbor.c`). A host implementation would either:
  - Reuse or port the nRF RPC + nRF RPC CBOR encoding/decoding (complex, depends on Zephyr), or
  - Reverse-engineer the minimal wire format and implement a small encoder/decoder for the DECT RPC commands and events only.

- **Commands (host → device, server receives)**:
  - `DECT_RPC_CMD_IF_SEND` (0): CBOR byte string = raw IPv6 packet.
  - `DECT_RPC_CMD_IF_GET_ADDRS`: No payload (or minimal). Response: addrs, prefixes, MTU, carrier, dormant (see client `apply_addrs_from_ctx` in `dect_rpc_if.c`).
  - `DECT_RPC_CMD_IF_STATUS`: Request has e.g. a sequence number (uint). Response: DECT status (modem, cluster, associations, FW version).
  - `DECT_RPC_CMD_PING`: No request payload; response = 0 (UART test).
  - `DECT_RPC_CMD_SHELL`: Request = `argc` (uint), then `argv[0]`..`argv[argc-1]` as CBOR byte strings. Response = status (0 OK, 1 BUSY). Shell output is pushed as events `DECT_RPC_CMD_SHELL_LINE`.

- **Events (device → host, server sends)**:
  - `DECT_RPC_CMD_IF_RECEIVE` (0): CBOR byte string = raw IPv6 packet.
  - `DECT_RPC_CMD_IF_LINK_STATE`: carrier (bool), dormant (bool).
  - `DECT_RPC_CMD_IF_ADDRS_CHANGED`: no payload; host should call GET_ADDRS to re-sync.
  - `DECT_RPC_CMD_SHELL_LINE`: one line of shell output (byte string).

- **Init order**: **Host (client) must be waiting first.** Then power up or reset the nRF9151 running the server. The server initializes the modem and then sends the RPC group-init; the host receives it, binds the group, and can then send/receive. (Same as Zephyr client/server: start client first, then server.)

### Implementation Options for the Host Client

1. **New userspace daemon in C or Rust**
   - Use a serial library (e.g. libserialport, or Linux termios) at 1 Mbps, RTS/CTS if desired.
   - Implement HDLC encode/decode, CRC-16-CCITT, and ACK handling as in `nrf_rpc_uart.c`.
   - Implement minimal nRF RPC packet encode/decode (group + CBOR) by either:
     - Extracting the relevant parts of nRF RPC + nRF RPC CBOR into a small library that builds on a non-Zephyr build (e.g. with a minimal CBOR library and a small RPC header layout), or
     - Writing a minimal encoder/decoder that only supports the DECT RPC command/event IDs and CBOR shapes used by DECT (byte string for IF_SEND/IF_RECEIVE, fixed structures for GET_ADDRS/STATUS, etc.).
   - Use Linux TUN device (e.g. `open("/dev/net/tun", ...)` with `IF_TUN`, `TUNSETIF`) and bridge packets to/from RPC.
   - Optional: D-Bus or a small CLI to trigger `dect run nw_join` / `dect run status` for network bring-up and diagnostics.

2. **Python prototype**
   - Same logic as above; `pyserial` for UART, `fcntl`/`struct` for TUN, or a library like `python-tun` for TUN.
   - Useful to validate the protocol and init order before committing to a C/Rust daemon.

3. **Reuse OpenThread RPC host tooling (if any)**
   - The DECT RPC design mirrors OpenThread RPC (see `nrf/subsys/net/openthread/rpc`). If Nordic or the community provides a host-side OpenThread RPC client that runs over UART, the same transport (HDLC, CRC, ACK) and possibly the same RPC serialization could be reused, with a DECT-specific command/event set. A search in the NCS tree did not find a ready-to-use host OpenThread RPC client; OpenThread harness tools use raw serial CLI, not the nRF RPC UART protocol.

### WSL2 / USB

- Under **WSL2**, USB devices are not visible by default. To attach the nRF9151 DK’s USB serial:
  - Use **usbipd** (e.g. `usbipd list`, `usbipd bind`, then on WSL `usbip attach`) to forward the USB device to WSL, or
  - Use a **native Linux** machine or a VM with USB passthrough.
- Once the device is visible in Linux, the host client opens `/dev/ttyACM0` (or the assigned node) like any other serial port.

## Alternative: PPP/SLIP Bridge on the Device

- **Idea**: Run firmware on the nRF9151 that bridges the **DECT net_if** to a **PPP** or **SLIP** stream on a second UART (or a dedicated “data” UART). The PC then runs standard **pppd** or **slattach** and gets a real network interface (e.g. `ppp0`) without implementing RPC on the host.
- **Pros**: No custom host RPC stack; standard Linux networking.
- **Cons**: Requires new firmware (and possibly a second UART or multiplexed link). The current NCS tree does not provide a DECT↔PPP or DECT↔SLIP bridge; this would be a new sample or a variant of dect_shell.


## Summary

| Approach | Host work | Device firmware | Result |
|----------|-----------|------------------|--------|
| **Host DECT RPC client** | Implement nRF RPC over UART + TUN bridge (and optionally shell-over-RPC) on Ubuntu | Existing **dect_rpc_server** | TUN interface on Ubuntu with IPv6 over DECT; control via RPC shell commands. |

The most direct way to let **Ubuntu (including WSL) use the DECT NR+ stack as a modem** today is to implement a **host-side DECT RPC client** that talks to the existing **dect_rpc_server** over USB serial and presents a **TUN** interface and optional control commands. The protocol (UART rate, HDLC, CRC, ACK, command/event IDs and CBOR shapes) is defined by the existing `nrf/subsys/net/dect/rpc` and `nrf/subsys/nrf_rpc` code; the main effort is implementing that on the host and adding the TUN bridge.
