.. _dect_rpc_server:

DECT NR+: RPC server (nRF9151)
###############################

.. contents::
   :local:
   :depth: 2

The DECT NR+ RPC server sample demonstrates a full DECT NR+ modem and stack on an **nRF9151 DK**, bridging IPv6 between the air interface and the :ref:`dect_rpc_client` over UART using nRF RPC.
It receives IPv6 from the client, injects it into DECT, and forwards DECT traffic back through the same tunnel.
Flash this sample to the kit that hosts DECT; pair it with the client sample on a second nRF9151 DK (see :ref:`dect_rpc_server_uart_wiring`).

Requirements
************

The sample supports the following development kit:

.. table-from-sample-yaml::

You also need an **nRF9151 DK** running :ref:`dect_rpc_client`, **jumper wires** for the RPC UART, and (for DECT over the air) DECT modem firmware and antenna as for other DECT samples.

.. include:: /includes/tfm.txt

Overview
********

The following summarizes the RPC link, DECT modem usage, IP bridging, and startup order.

* **UART1** is the default RPC link (**1 Mbps**, RTS/CTS) on the **same pins** as :file:`dect_shell/boards/nrf9151dk_nrf9151_ns_sm.overlay`: **P0.11** TX, **P0.10** RX, **P0.13** RTS, **P0.12** CTS. Default UART RPC is **non-reliable**; use :file:`rpc-uart-reliable.conf` on **both** kits (with the client) for per-frame ACK.
* On-chip DECT modem uses **IPC**, not UART — no clash with RPC UART1 when modem trace to UART is off (``CONFIG_NRF_MODEM_LIB_TRACE=n``).
* **No mDNS/DNS resolver** in default ``prj.conf``: this image only bridges IPv6 between DECT and the RPC UART. mDNS for ``*.local`` runs on the :ref:`dect_rpc_client` when you use its :file:`mdns.conf` (see :ref:`dect_rpc_server_mdns`).
* **Init order:** the **client** must be running in ``nrf_rpc_init()`` **before** this server sends the group init (see :ref:`dect_rpc_server_rpc_init`).

Configuration
*************

|config|

Default ``prj.conf`` and board devicetree follow the `dect_mac RPC client/server reference <https://github.com/jhirsi/sdk-nrf/commit/b5dfb04e3d1ddbc6e82505a0201396ae719cb472>`_.
To move RPC off UART1, set ``nordic,rpc-uart = &uart0`` in devicetree and relocate the console accordingly; wire the client's RPC UART to that server UART.

Configuration files
=====================

The sample provides additional Kconfig fragments in the sample directory for optional features.

The following files are available:

* :file:`debug-net.conf` — Verbose Zephyr network stack logging (see :ref:`dect_rpc_server_netdbg`).
* :file:`rpc-uart-reliable.conf` — Reliable nRF RPC UART transport; build **both** server and client with this fragment when you use it (see :ref:`dect_rpc_client_extra_rpc_uart_reliable`).

.. _dect_rpc_server_building:

Building and running
********************

.. |sample path| replace:: :file:`samples/dect/dect_rpc_server`

.. include:: /includes/build_and_run_ns.txt

.. note::
   Same as :ref:`dect_shell_application`: ``cd`` to this sample under your |NCS| workspace, then ``west build`` with no source path on the command line.

See :ref:`cmake_options` for CMake options.

nRF9151 DK (server)
===================

.. code-block:: console

   nrf/samples/dect/dect_rpc_server:
   west build -p -b nrf9151dk/nrf9151/ns && west flash

RPC client (nRF9151)
====================

.. code-block:: console

   nrf/samples/dect/dect_rpc_client:
   west build -p -b nrf9151dk/nrf9151/ns && west flash

.. _dect_rpc_server_netdbg:

Net stack debug (optional)
==========================

.. code-block:: console

   nrf/samples/dect/dect_rpc_server:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE=debug-net.conf && west flash

UART RPC reliable mode (optional)
==================================

Same as :ref:`dect_rpc_client_extra_rpc_uart_reliable`: default builds omit per-frame ACK;
add :file:`rpc-uart-reliable.conf` when building **both** server and client.

.. code-block:: console

   nrf/samples/dect/dect_rpc_server:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE=rpc-uart-reliable.conf && west flash

Testing
=======

|test_sample|

Two-board setup
===============

#. Wire the kits as in :ref:`dect_rpc_server_uart_wiring`.
#. Flash :ref:`dect_rpc_client` to the client kit; flash this sample to the nRF9151 server kit.
#. |connect_kit| and |connect_terminal| on both (**115200** 8N1).
#. Reset the **client** first until it waits for the server; then reset the **server**.
#. Use ``dect status`` and DECT shell commands on the server as in other DECT samples; exercise IPv6 via the RPC path from the client.

.. _dect_rpc_server_uart_wiring:

UART wiring (RPC link)
**********************

Baud rate **1 Mbps** with **RTS/CTS**; connect **UART1** to the client's RPC UART (TX↔RX and RTS↔CTS crossed, plus GND).
Pinout matches :file:`dect_shell/boards/nrf9151dk_nrf9151_ns_sm.overlay` (Serial Modem DK wiring) so the same harness can be reused with :ref:`dect_shell_application` SM builds.
The table lists the **server** column first; see :ref:`dect_rpc_client` for the same map from the client side.

.. table:: Wire two nRF9151 DKs as RPC server and RPC client.

   +---------------------------+---------------------------+
   | RPC server                | RPC client                |
   +---------------------------+---------------------------+
   | nRF9151 DK                | nRF9151 DK                |
   |                           |                           |
   +===========================+===========================+
   | **P0.10** (RX)            | **P0.11** (TX)            |
   +---------------------------+---------------------------+
   | **P0.11** (TX)            | **P0.10** (RX)            |
   +---------------------------+---------------------------+
   | **P0.12** (CTS)           | **P0.13** (RTS)           |
   +---------------------------+---------------------------+
   | **P0.13** (RTS)           | **P0.12** (CTS)           |
   +---------------------------+---------------------------+
   | GND                       | GND                       |
   +---------------------------+---------------------------+

.. _dect_rpc_server_rpc_init:

RPC initialization
********************

RPC is initialized in **main()** (no shell command).
The **client** must be started **first** and blocking in ``nrf_rpc_init()``; then start **this server** so it can send the group init after modem init.

If the server starts with no waiting client, it fails after ~15 s — power-cycle and repeat **client first**, then server.

.. _dect_rpc_server_mdns:

mDNS
****

This sample does **not** enable mDNS responder or DNS resolver on the server.

**Who joins multicast (ff02::fb)?** The host that **sends** mDNS queries or **answers** as an mDNS peer must join **MLD** for ``ff02::fb`` on the interface where that traffic belongs. On the **RPC client**, that is the client logical iface name (default **rpc-dect**;
``CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME``) when you build with :file:`mdns.conf` (resolver + responder). On **DECT peers** (for example a sink running DeSh), the join is on **dect0**. This server **forwards** IPv6 between those interfaces; it does **not** need to join ``ff02::fb`` itself unless you extend devicetree or Kconfig and want the server shell to run ``net dns … .local`` or to advertise a name on DECT.

Troubleshooting
***************

If ``dect sett`` fails with **"Cannot write new settings: -2"** or **failure on scope**, settings could not be persisted (**ENOENT** — no backend).

* Ensure ``CONFIG_FLASH``, ``CONFIG_FLASH_MAP``, ``CONFIG_MPU_ALLOW_FLASH_WRITE``, ``CONFIG_SETTINGS``, and ``CONFIG_SETTINGS_NVS`` are enabled (this sample sets them; verify ``.config`` after build).
* Retry ``dect sett`` once after boot if init order caused a first-write miss.

Debugging (net stack traces)
****************************

See :ref:`dect_rpc_server_netdbg` in :ref:`dect_rpc_server_building`.

Dependencies
************

This sample uses the following Zephyr libraries:

* :ref:`zephyr:networking_api` — IP bridging between DECT and the RPC net interface.

It uses the DECT modem and stack on nRF9151 (see :file:`prj.conf` and the sample :file:`Kconfig`).

Run it together with :ref:`dect_rpc_client` on a second nRF9151 DK (see :ref:`dect_rpc_server_uart_wiring`).
