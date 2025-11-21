.. _dect_rpc_server:

DECT RPC Server Sample
######################

Overview
********

This sample demonstrates a DECT NR+ RPC server that exposes DECT NR+ net mgmt
API over RPC. The server runs the full DECT NR+ stack and accepts RPC
commands from a remote client device.

The server supports two transport modes:

1. **UART Transport**: Direct device-to-device communication over UART
2. **MQTT Transport**: Cloud-to-device communication via nRF Cloud MQTT

The server handles:

- DECT stack activation/deactivation
- Status information queries
- Settings read/write
- Event forwarding to client

Requirements
************

**For UART Transport:**
- nRF9151DK (server device)
- Another nRF9151DK running the DECT RPC Client sample (client device)
- UART connection between the two devices (UART1, 1 Mbps)

**For MQTT Transport:**
- nRF9151DK (server device)
- nRF Cloud account and device provisioning
- Network connectivity (cellular or Wi-Fi)
- MQTT client (nRF Cloud web portal, REST API, or custom client)

Hardware Setup
**************

**For UART Transport:**

1. Connect the two nRF9151DK devices via UART1:

   - Server UART1 TX (P0.29) → Client UART1 RX (P0.28)
   - Server UART1 RX (P0.28) → Client UART1 TX (P0.29)
   - Server UART1 RTS (P0.16) → Client UART1 CTS (P0.17)
   - Server UART1 CTS (P0.17) → Client UART1 RTS (P0.16)
   - GND → GND

   **Note**: P0.28 and P0.29 are not directly accessible on the Arduino header.
   They may be available as test points or through the Arduino connector's
   UART function. Consult the nRF9151 DK Hardware User Guide (PCA10171) for
   the exact pin locations and schematics.

2. Power both devices

**For MQTT Transport:**

1. Ensure the nRF9151DK has network connectivity (cellular or Wi-Fi)
2. Provision the device with nRF Cloud credentials
3. Power the device

Building and Running
********************

**Basic Build (UART Transport - Default):**

The default configuration uses UART transport:

.. code-block:: console

   west build -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_server
   west flash

**UART Transport with Explicit Overlay:**

You can also explicitly use the UART overlay:

.. code-block:: console

   west build -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_server -- -DOVERLAY_CONFIG=overlay-uart.conf
   west flash

**MQTT Transport Configuration:**

Use the MQTT overlay to enable cloud-based RPC communication:

.. code-block:: console

   west build -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_server -- -DOVERLAY_CONFIG=overlay-mqtt.conf
   west flash

**Build with Debug Logging:**

Enable verbose debug logging for DECT NR+ stack and RPC transport:

.. code-block:: console

   # UART transport with debug logging
   west build -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_server -- -DOVERLAY_CONFIG="overlay-uart.conf;overlay-debug.conf"
   west flash

   # MQTT transport with debug logging
   west build -b nrf9151dk/nrf9151/ns nrf/samples/dect/dect_mac/dect_rpc_server -- -DOVERLAY_CONFIG="overlay-mqtt.conf;overlay-debug.conf"
   west flash

The debug overlay (``overlay-debug.conf``) enables:
- Debug logging for all DECT NR+ stack components (MAC, L2, Management, Border Router, Connection Manager)
- Debug logging for nRF RPC transport and core
- Debug logging for UART (when using UART transport)
- Increased log buffer size (16KB)

**Note:** The overlay files (``overlay-uart.conf``, ``overlay-mqtt.conf``, and ``overlay-debug.conf``) handle
all the necessary configuration automatically. You don't need to manually
edit ``prj.conf`` when using overlays.

**Asserts:**

Asserts are enabled by default in ``prj.conf``:
- ``CONFIG_ASSERT=y`` - Enable assertions
- ``CONFIG_ASSERT_LEVEL=2`` - Assert level 2 (strict)
- ``CONFIG_ASSERT_ON_ERRORS=y`` - Assert on errors

To disable asserts, comment out or remove these lines in ``prj.conf``.

Testing
*******

**UART Transport:**

1. Wait for the server to initialize. You should see:

   .. code-block:: console

      [00:00:00.xxx] <inf> dect_rpc_server_sample: DECT RPC server ready
      [00:00:00.xxx] <inf> dect_rpc_server_sample: Waiting for RPC commands from client...

2. The server will automatically handle RPC commands from the client.

3. DECT events will be forwarded to the client automatically.

**MQTT Transport:**

1. Wait for the server to initialize and connect to nRF Cloud. You should see:

   .. code-block:: console

      [00:00:00.xxx] <inf> dect_rpc_server_sample: Initializing nRF Cloud MQTT
      [00:00:00.xxx] <inf> dect_rpc_server_sample: Connecting to nRF Cloud...
      [00:00:02.xxx] <inf> dect_rpc_server_sample: nRF Cloud MQTT connected
      [00:00:02.xxx] <inf> dect_rpc_server_sample: nRF Cloud ready
      [00:00:02.xxx] <inf> dect_rpc_server_sample: DECT RPC server ready
      [00:00:02.xxx] <inf> dect_rpc_server_sample: Waiting for RPC commands from cloud via MQTT...

2. Send DECT RPC commands via nRF Cloud (REST API, web portal, or MQTT client)

3. The server will automatically handle RPC commands from the cloud.

4. DECT events will be forwarded to the cloud automatically.

Configuration
*************

The sample uses the following key configurations:

**Common:**
- ``CONFIG_DECT_NRP_MAC=y`` - Enable DECT NR+ MAC
- ``CONFIG_DECT_NRP_MAC_DRIVER_NRF=y`` - Use nRF91 driver
- ``CONFIG_DECT_NRP_RPC_SERVER=y`` - Enable DECT RPC server

**UART Transport:**
- ``CONFIG_NRF_RPC_UART_TRANSPORT=y`` - UART transport for RPC
- ``CONFIG_NRF_RPC_UART_RELIABLE=y`` - Enable reliable UART transport

**MQTT Transport:**
- ``CONFIG_NRF_CLOUD=y`` - Enable nRF Cloud
- ``CONFIG_NRF_CLOUD_MQTT=y`` - Enable nRF Cloud MQTT
- ``CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT=y`` - MQTT transport for RPC

See ``prj.conf`` for full configuration options.

**Note:** Only one transport can be enabled at a time. The transport is selected
via Kconfig choice ``DECT_NRP_RPC_TRANSPORT``.

