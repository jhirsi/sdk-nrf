.. _dect_rpc_sample:

.. ncs-sample::
   :title: nRF54L15 / nRF91: DECT NR+ net_if on external MCU (experimental)

   This sample shows how an external MCU without a DECT NR+ modem can use a normal Zephyr DECT NR+ network interface while an nRF9151 runs the modem and stack over UART nRF RPC.
   It uses the :ref:`dect_rpc` solution.

.. note::
   DECT NR+ RPC and this sample are :ref:`experimental <software_maturity>`.
   Enable :kconfig:option:`CONFIG_DECT_NR_RPC` (menu label includes ``[EXPERIMENTAL]``) and expect API and protocol changes.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

You need two of the development kits listed above (default nRF54L15 DK as RPC client and nRF9151 DK as RPC server), jumper wires for the RPC UART (see :ref:`dect_rpc_client_uart_wiring_nrf54l15`), and (for over-the-air DECT) DECT NR+ modem firmware and antennas on the server kit.

When you build for nRF9151 DK (``*/ns`` target), the application uses Trusted Firmware-M:

.. include:: /includes/tfm.txt

Overview
********

One sample tree builds two firmware images: an RPC client on the external MCU (default ``prj.conf`` on nRF54L15 DK) and an RPC server on nRF9151 DK (:file:`server.conf`).
The client exposes ``dect0`` locally; the server runs ``l2_dect`` and the modem and mirrors interface state and IPv6 over UART RPC at 1 Mbps with RTS/CTS.

RPC is initialized in main() on each side (see :ref:`dect_rpc_client_rpc_init`): start the client before the server.

For architecture and protocol details, see :ref:`dect_rpc`.

Quick start tutorial
====================

Step 1: Wire and flash

Wire the nRF54L15 DK (client) to the nRF9151 DK (server) as in :ref:`dect_rpc_client_uart_wiring_nrf54l15`.
Flash the server image on the nRF9151 DK (:ref:`dect_rpc_build_server`) and the client image on the nRF54L15 DK (:ref:`dect_rpc_build_client`).

Step 2: RPC bring-up (client first)

Reset the client until the log shows it is waiting for the server, then reset the server.
When both sides log that RPC is initialized, run ``dect rpc ping`` on the client (``DECT NR+ RPC UART test: OK``).

Step 3: IP and DECT

Wait for auto address sync (~``CONFIG_DECT_NR_RPC_AUTO_SYNC_DELAY_SEC`` s after RPC init) or run ``dect sync`` on the client.
Use ``dect connect`` on the client to associate DECT on the server, then ``dect status`` on the client.
For ICMPv6, rebuild the client with :file:`ping.conf` (and :file:`dns.conf` for hostnames) and use ``ping -d <destination>``.

Wiring
******

.. _dect_rpc_client_uart_wiring:

Baud rate is 1000000 (1 Mbps) with hardware flow control (RTS/CTS): four signals plus GND.
Connect by signal (TX↔RX), not by pin label alone when wiring kits.

The default setup is nRF54L15 DK (client) → nRF9151 DK (server).
An optional nRF9151 + nRF9151 setup uses UART1 on both (same pinmux as :ref:`dect_shell_application` Serial Modem overlay).

.. _dect_rpc_client_uart_wiring_nrf54l15:

nRF54L15 DK client to nRF9151 DK server
=======================================

.. table:: Wire an nRF54L15 DK application core (RPC client) to an nRF9151 DK (RPC server).

   +---------------------------+---------------------------+
   | RPC client (nRF54L15 DK)  | RPC server (nRF9151 DK)   |
   +===========================+===========================+
   | P1.08 (TX)                | P0.10 (RX)                |
   +---------------------------+---------------------------+
   | P1.09 (RX)                | P0.11 (TX)                |
   +---------------------------+---------------------------+
   | P1.11 (RTS)               | P0.12 (CTS)               |
   +---------------------------+---------------------------+
   | P1.12 (CTS)               | P0.13 (RTS)               |
   +---------------------------+---------------------------+
   | GND                       | GND                       |
   +---------------------------+---------------------------+

Two nRF9151 DKs (alternate client and server)
=============================================

.. table:: Optional: wire two nRF9151 DKs as RPC client and RPC server.

   +---------------------------+---------------------------+
   | RPC client                | RPC server                |
   +---------------------------+---------------------------+
   | nRF9151 DK                | nRF9151 DK                |
   |                           |                           |
   +===========================+===========================+
   | P0.11 (TX)                | P0.10 (RX)                |
   +---------------------------+---------------------------+
   | P0.10 (RX)                | P0.11 (TX)                |
   +---------------------------+---------------------------+
   | P0.13 (RTS)               | P0.12 (CTS)               |
   +---------------------------+---------------------------+
   | P0.12 (CTS)               | P0.13 (RTS)               |
   +---------------------------+---------------------------+
   | GND                       | GND                       |
   +---------------------------+---------------------------+

Use all signal rows plus GND for reliable flow control.

.. _dect_rpc_client_rpc_init:

RPC initialization (both roles)
=================================

RPC is initialized in main() (no shell command).
The client blocks in ``nrf_rpc_init()`` until the server sends the group init after modem init.

Order (required):

1. Start the client first — wait for the log line that says the client is waiting for the server.
2. Then start the server — after modem init it sends the group init; the client unblocks.

If the server starts first, it fails after about 15 seconds.
Reset both kits and repeat with the client first.

User interface
**************

.. _dect_rpc_client_role:

RPC client
==========

The client image uses default ``prj.conf`` (or the same options on nRF9151 without :file:`server.conf`).
It does not run the DECT NR+ modem; it mirrors the server's DECT interface state and forwards IPv6 over RPC so sockets and the IP stack run locally on the external MCU.

Console: USB serial 115200 8N1.
RPC UART: UART21 on nRF54L15 (P1.08–P1.12); UART1 on nRF9151 client (P0.10–P0.13).

Shell and logs use the kit USB console.
RPC uses a separate UART (see :ref:`dect_rpc_client_uart_wiring`).

* ``dect rpc ping`` — RPC UART loopback test to the server.
* ``dect sync`` — Pull IPv6 addresses from the server and bring ``dect0`` up.
* ``dect connect`` / ``dect disconnect`` — Start or stop DECT NR+ association on the server (``CONFIG_DECT_NR_RPC_CONN_MGR``, default y).
* ``dect status`` — DECT NR+ status from the server (dedicated RPC path).
* ``dect run <subcmd> [args...]`` — Run a DECT L2 shell command on the server (for example ``dect run nw_join``). Use ``dect status`` for status, not ``dect run status``. Does not include ICMP.
* ``ping -d <destination>`` — ICMPv6 on ``dect0`` (requires :file:`ping.conf`; add :file:`dns.conf` for hostnames).

Connection Manager on the client raises ``NET_EVENT_L4_*`` when the mirrored interface has a global IPv6 address.
DECT association policy (``NET_L2_DECT_CONN_MGR_*``) is configured on the server in ``server.conf`` only.

.. _dect_rpc_server_role:

RPC server
==========

The server image runs on nRF9151 DK only.
Use :file:`server.conf` and :file:`overlay-nrf9151-server-modem.overlay` (modem IPC memory layout on top of the default client board overlay).

Console: USB serial 115200 8N1.
RPC UART: UART1 P0.10–P0.13 (same as client when both are nRF9151).

The server runs the local DECT NR+ L2 shell (as in :ref:`dect_shell_application`).
Use ``dect help`` on the server terminal for the full list.

The server does not expose ``dect sync`` or ``dect0`` — those exist on the client image.
IPv6 is bridged between DECT and the RPC link; the client mirrors addresses on ``dect0``.

Configuration
*************

|config|

Configuration options
=====================

Check and configure the following Kconfig options:

.. options-from-kconfig::
   :show-type:

Configuration files
===================

The sample provides predefined configuration files for typical use cases.
You can find them in the sample directory.

* :file:`prj.conf` — Default RPC client (nRF54L15 or nRF9151 without modem).
* :file:`server.conf` — RPC server on nRF9151 (DECT NR+ modem, conn_mgr on DECT NR+).
* :file:`overlay-nrf9151-server-modem.overlay` — Required with :file:`server.conf` (modem SHMEM).
* :file:`dns.conf`, :file:`ping.conf` — Optional client networking (see :ref:`dect_rpc_build_client`).
* :file:`debug-net.conf` — Verbose net stack logging (client or server).
* :file:`rpc-large-rx.conf` — Larger RPC UART RX ring on the client.
* :file:`iperf3-common.conf`, :file:`iperf3-tx.conf`, :file:`iperf3-rx.conf` — iperf3 on the client.

Additional configuration
========================

Check and configure the following library options that are used by the sample:

* :kconfig:option:`CONFIG_DECT_NR_RPC` — Enables DECT NR+ RPC (experimental).
* :kconfig:option:`CONFIG_DECT_NR_RPC_CLIENT` / :kconfig:option:`CONFIG_DECT_NR_RPC_SERVER` — Client or server role.
* :kconfig:option:`CONFIG_DECT_NR_RPC_NET_IF` — Network interface and packet bridge (must match on both sides).
* :kconfig:option:`CONFIG_NRF_RPC` and :kconfig:option:`CONFIG_NRF_RPC_UART_TRANSPORT` — UART transport between kits.

RPC UART 1 Mbps with RTS/CTS is set in :file:`boards/nrf54l15dk_nrf54l15_cpuapp.overlay` and :file:`boards/nrf9151dk_nrf9151_ns.overlay`.
See :file:`subsys/net/dect/rpc/Kconfig` for tunneled packet size, event pacing, and shell options.

Building and running
********************

.. |sample path| replace:: :file:`samples/dect/dect_rpc`

.. include:: /includes/build_and_run.txt

.. note::
   Match :ref:`dect_shell_application`: ``cd`` to this sample inside your |NCS| workspace, then ``west build`` with no source directory argument on the command line.

See :ref:`cmake_options` for instructions on how to provide CMake options, for example ``-DEXTRA_CONF_FILE`` and ``-DEXTRA_DTC_OVERLAY_FILE``.

.. _dect_rpc_build_client:

Building the RPC client
=======================

Board files under :file:`boards/` default to client devicetree on nRF9151 (:file:`nrf9151dk_nrf9151_ns.overlay`; modem disabled in :file:`nrf9151dk_nrf9151_ns.conf`).

nRF54L15 DK (default)
---------------------

.. code-block:: console

   nrf/samples/dect/dect_rpc:
   west build -p -d build/client -b nrf54l15dk/nrf54l15/cpuapp && west flash -d build/client

nRF9151 DK (alternate client)
-------------------------------

.. code-block:: console

   nrf/samples/dect/dect_rpc:
   west build -p -d build/client -b nrf9151dk/nrf9151/ns && west flash -d build/client

.. _dect_rpc_client_extra_ping_dns:

ICMP ping with DNS (client, optional)
-------------------------------------

:file:`ping.conf` enables the ``ping`` shell command.
Use with :file:`dns.conf` for hostnames.

.. code-block:: console

   west build -p -d build/client -b nrf54l15dk/nrf54l15/cpuapp -- -DEXTRA_CONF_FILE="ping.conf;dns.conf" && west flash -d build/client

.. _dect_rpc_client_extra_iperf3:

iperf3 (client, optional)
-------------------------

See :ref:`dect_shell_application` iperf3 support for usage over DECT after ``dect connect``.

.. code-block:: console

   west build -p -d build/client -b nrf54l15dk/nrf54l15/cpuapp -- -DEXTRA_CONF_FILE="iperf3-common.conf;iperf3-tx.conf" && west flash -d build/client

Expected throughput (UDP over RPC)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The RPC link uses a 1 Mbps UART, but UDP throughput on ``dect0`` is much lower than 1 Mbit/s.
Each UDP datagram on ``dect0`` waits on one blocking ``IF_SEND`` RPC round-trip before the next ``send()`` can finish.
With typical iperf payloads (for example ``-l 1220``), steady client throughput is about 521–522 kbit/s (~53 datagrams/s).
The same iperf command on DeSh without RPC is typically about 1.1 Mbit/s to a parent FT.

.. _dect_rpc_build_server:

Building the RPC server
=======================

.. code-block:: console

   nrf/samples/dect/dect_rpc:
   west build -p -d build/server -b nrf9151dk/nrf9151/ns -- \
     -DEXTRA_CONF_FILE=server.conf \
     -DEXTRA_DTC_OVERLAY_FILE=overlay-nrf9151-server-modem.overlay && west flash -d build/server

Testing
=======

|test_sample|

#. Wire the kits as in :ref:`dect_rpc_client_uart_wiring`.
#. Flash the server image (:ref:`dect_rpc_build_server`) on the nRF9151 DK and the client image (:ref:`dect_rpc_build_client`) on the other kit.
#. |connect_kit| and |connect_terminal| on both (115200 8N1).
#. Reset the client first, then the server (:ref:`dect_rpc_client_rpc_init`).
#. On the client, run ``dect rpc ping``; after sync, ``dect status``.
#. On the server, use local ``dect`` / ``net`` commands as needed for your DECT scenario.

Troubleshooting
***************

RPC link (both roles)
=====================

If you see ``nrf_rpc_uart: Ack timeout``, ``RPC send failed``, or on the server ``nrf_rpc_init failed: -35`` / ``Failed to send group init packet ... err: -71``:

Most common cause: the server was started before the client.

1. Init order: Client waiting log first, then server until both log ``RPC initialized``.
2. Wiring: TX↔RX, RTS↔CTS, GND (see :ref:`dect_rpc_client_uart_wiring_nrf54l15`).
3. Baud / UART: 1 Mbps; UART21 (54L15 client) or UART1 (nRF9151).
4. Debug: For RPC UART, set ``CONFIG_NRF_RPC_TR_LOG_LEVEL_DBG=y`` and ``CONFIG_NRF_RPC_LOG_LEVEL_DBG=y`` in ``prj.conf`` or ``server.conf``; for net stack tracing, append :file:`debug-net.conf` to ``EXTRA_CONF_FILE`` (client or ``server.conf;debug-net.conf`` on the server).

Dependencies
************

This sample uses the following |NCS| libraries and subsystems:

* :ref:`dect_rpc` — Split-MCU DECT NR+ RPC (client ``dect0``; server runs the DECT NR+ L2 stack and modem and bridges to RPC).
* :ref:`nrf_rpc` — UART transport and CBOR RPC between kits.

It uses the following Zephyr libraries:

* :ref:`zephyr:networking_api` — IPv6 on the client ``dect0`` interface.

In addition, it uses the following secure firmware component:

* :ref:`Trusted Firmware-M <ug_tfm>` (nRF9151 ``*/ns`` builds)
