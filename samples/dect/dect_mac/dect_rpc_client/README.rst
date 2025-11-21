.. _dect_rpc_client:

DECT RPC Client Sample
######################

Overview
********

This sample demonstrates a DECT NR+ RPC client that can control a DECT NR+ stack
running on a remote device over RPC.

The client supports two transport modes:

1. **UART Transport**: Direct device-to-device communication over UART
2. **MQTT Transport**: Cloud-to-device communication via nRF Cloud MQTT (not yet implemented in client)

The client provides shell commands to:

- Activate/deactivate DECT stack
- Read status information
- Read settings
- Subscribe to DECT events

Requirements
************

**For UART Transport:**
- nRF9151DK (client device)
- Another nRF9151DK running the DECT RPC Server sample (server device)
- UART connection between the two devices (UART1, 1 Mbps)

**For MQTT Transport:**
- nRF9151DK (client device)
- nRF Cloud account and device provisioning
- Network connectivity (cellular or Wi-Fi)
- Server device running with MQTT transport enabled

Hardware Setup
**************

1. Connect the two nRF9151DK devices via UART1:

   - Client UART1 TX (P0.29) → Server UART1 RX (P0.28)
   - Client UART1 RX (P0.28) → Server UART1 TX (P0.29)
   - Client UART1 RTS (P0.16) → Server UART1 CTS (P0.17)
   - Client UART1 CTS (P0.17) → Server UART1 RTS (P0.16)
   - GND → GND

   **Note**: P0.28 and P0.29 are not directly accessible on the Arduino header.
   They may be available as test points or through the Arduino connector's
   UART function. Consult the nRF9151 DK Hardware User Guide (PCA10171) for
   the exact pin locations and schematics.

2. Power both devices

Building and Running
********************

**Basic Build:**

1. Build the server sample first (see :ref:`dect_rpc_server` for server build instructions):

   .. code-block:: console

      west build -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_server
      west flash

2. Build the client sample:

   .. code-block:: console

      west build -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_client
      west flash

3. Connect to both devices with terminal emulators (115200 baud)

**Build with Debug Logging:**

Enable verbose debug logging for DECT NR+ stack and RPC transport:

.. code-block:: console

   west build -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_client -- -DOVERLAY_CONFIG=overlay-debug.conf
   west flash

The debug overlay (``overlay-debug.conf``) enables:
- Debug logging for all DECT NR+ stack components (MAC, L2, Management, Border Router, Connection Manager)
- Debug logging for nRF RPC transport and core
- Debug logging for UART
- Increased log buffer size (16KB)

**MQTT Transport Configuration:**

Note: MQTT transport for the client is not yet implemented. The server can be configured
with MQTT transport, but the client currently only supports UART transport.

**Asserts:**

Asserts are enabled by default in ``prj.conf``:
- ``CONFIG_ASSERT=y`` - Enable assertions
- ``CONFIG_ASSERT_LEVEL=2`` - Assert level 2 (strict)
- ``CONFIG_ASSERT_ON_ERRORS=y`` - Assert on errors

To disable asserts, comment out or remove these lines in ``prj.conf``.

Testing
*******

1. Wait for both devices to initialize. You should see:

   **Server:**
   .. code-block:: console

      [00:00:00.xxx] <inf> dect_rpc_server_sample: DECT RPC server ready
      [00:00:00.xxx] <inf> dect_rpc_server_sample: Waiting for RPC commands from client...

   **Client:**
   .. code-block:: console

      [00:00:00.xxx] <inf> dect_rpc_client_sample: DECT RPC client ready
      [00:00:00.xxx] <inf> dect_rpc_client_sample: Use shell commands to control DECT stack

2. On the client terminal, use shell commands:

   .. code-block:: console

      uart:~$ dect activate
      DECT stack activated

      uart:~$ dect status
      DECT Status:
        Modem activated: yes
        Cluster running: no
        Network beacon: no
        Parent count: 0
        Child count: 0
        FW version: ...

      uart:~$ dect settings_read
      DECT Settings:
        Device type: 0x00000001
        Network ID: 0x12345678
        Long RD ID: 0xabcdef00
        Region: 0
        Band: 0
        Max TX power: 10 dBm
        Power save: disabled

      uart:~$ dect deactivate
      DECT stack deactivated

3. Events from the server will be automatically received and logged:

   .. code-block:: console

      [00:00:05.xxx] <inf> dect_rpc_client_sample: DECT Event: iface_index=1, event=0x80015701

Configuration
*************

The sample uses the following key configurations:

- ``CONFIG_NRF_RPC_UART_TRANSPORT=y`` - UART transport for RPC
- ``CONFIG_DECT_NRP_RPC_CLIENT=y`` - Enable DECT RPC client
- ``CONFIG_SHELL=y`` - Enable shell for testing

See ``prj.conf`` for full configuration options.

