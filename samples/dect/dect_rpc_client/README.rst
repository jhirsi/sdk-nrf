.. _dect_rpc_client:

DECT NR+: RPC client
#####################

.. contents::
   :local:
   :depth: 2

The DECT NR+ RPC client sample demonstrates IPv6 forwarding between Zephyr and an nRF9151 DK running the :ref:`dect_rpc_server`, without a DECT NR+ modem on the client board.
It exposes a Zephyr network interface (``dect_rpc``) and tunnels traffic over UART using nRF RPC.
Flash this sample to an **nRF9151 DK** acting as the RPC client; the DECT stack and modem run only on the server kit.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

You also need an **nRF9151 DK** running the :ref:`dect_rpc_server` sample, **jumper wires** for the RPC UART between client and server (see :ref:`dect_rpc_client_uart_wiring`), and (for over-the-air DECT tests) the modem firmware and antennas as described for the server.

.. include:: /includes/tfm.txt

Overview
********

The following summarizes behavior, default options, and related documentation.

* Forwards IPv6 between Zephyr ``dect_rpc`` and the server over **UART** at **1 Mbps** with **RTS/CTS**.
* Default **UART RPC** is **non-reliable** (no per-frame HDLC ACK) for throughput; use :ref:`dect_rpc_client_extra_rpc_uart_reliable` on **both** kits for noisy links.
* **Shell** and logs use **UART0** (**115200** 8N1); **RPC** uses **UART1** with the same **pinmux** as :file:`dect_shell/boards/nrf9151dk_nrf9151_ns_sm.overlay` (see :ref:`dect_rpc_client_uart_wiring`).
* **mDNS** advertise/query (**dect-rpc-client.local**, etc.): enable :ref:`dect_rpc_client_extra_mdns` (default ``prj.conf`` has unicast DNS only).
* RPC and link bring-up are driven from **main()** (see :ref:`dect_rpc_client_rpc_init`): start the **client** before the **server**.

Configuration
*************

|config|

Board-specific UART and ``nordic,rpc-uart`` are set under :file:`boards/`.
Default ``prj.conf`` follows the `dect_mac RPC client/server reference <https://github.com/jhirsi/sdk-nrf/commit/b5dfb04e3d1ddbc6e82505a0201396ae719cb472>`_.

Configuration files
=====================

The sample provides additional Kconfig fragments in the sample directory for optional features.

The following files are available:

* :file:`mdns.conf` — Full mDNS stack on the client (see :ref:`dect_rpc_client_extra_mdns`).
* :file:`debug-net.conf` — Verbose Zephyr network stack logging (see :ref:`dect_rpc_client_netdbg`).
* :file:`rpc-uart-reliable.conf` — Reliable nRF RPC UART transport; build **both** client and server with this fragment when you use it (see :ref:`dect_rpc_client_extra_rpc_uart_reliable`).
* :file:`iperf3-common.conf` with :file:`iperf3-tx.conf` or :file:`iperf3-rx.conf` — Nordic iperf3 integration for throughput tests (see :ref:`dect_rpc_client_extra_iperf3`).

.. _dect_rpc_client_building:

Building and running
********************

.. |sample path| replace:: :file:`samples/dect/dect_rpc_client`

.. include:: /includes/build_and_run_ns.txt

.. note::
   Match :ref:`dect_shell_application`: open a terminal, ``cd`` to this sample inside your |NCS| workspace (path below), then run ``west build`` with **no** source directory argument on the command line.

See :ref:`cmake_options` for passing CMake options (for example ``-DEXTRA_CONF_FILE``).

nRF9151 DK (client)
===================

.. code-block:: console

   nrf/samples/dect/dect_rpc_client:
   west build -p -b nrf9151dk/nrf9151/ns && west flash

RPC server (nRF9151, other board)
=================================

.. code-block:: console

   nrf/samples/dect/dect_rpc_server:
   west build -p -b nrf9151dk/nrf9151/ns && west flash

.. _dect_rpc_client_netdbg:

Net stack debug (optional)
==========================

.. code-block:: console

   nrf/samples/dect/dect_rpc_client:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE=debug-net.conf && west flash

If debug lines are missing, do a clean build (``rm -rf build`` in this directory) and check ``build/dect_rpc_client/zephyr/.config`` (or ``build/.../zephyr/.config`` for sysbuild) for ``CONFIG_NET_LOG=y``.

.. _dect_rpc_client_extra_mdns:

mDNS (optional)
===============

Default ``prj.conf`` enables **unicast DNS** only (``CONFIG_DNS_SERVER1``).
:file:`mdns.conf` adds the full mDNS stack: **responder** + **DNS-SD** + **hostname** (e.g. ``dect-rpc-client.local``) and **resolver** (``*.local`` queries to **ff02::fb**, RFC 6762), for example ``net dns <host>.local AAAA``.

.. code-block:: console

   nrf/samples/dect/dect_rpc_client:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE=mdns.conf && west flash

Combine extra configuration files with a semicolon, for example ``-- -DEXTRA_CONF_FILE="mdns.conf;debug-net.conf"``.

.. _dect_rpc_client_extra_rpc_uart_reliable:

UART RPC reliable mode (optional)
=================================

Default ``prj.conf`` uses ``CONFIG_NRF_RPC_UART_RELIABLE=n`` on client and server (higher throughput).
:file:`rpc-uart-reliable.conf` enables **per-frame ACK** and retries; apply the **same** fragment when building **both** images so UART settings match.

.. code-block:: console

   nrf/samples/dect/dect_rpc_client:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE=rpc-uart-reliable.conf && west flash

.. code-block:: console

   nrf/samples/dect/dect_rpc_server:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE=rpc-uart-reliable.conf && west flash

.. _dect_rpc_client_extra_iperf3:

iperf3 (optional)
=================

Throughput testing uses the same Nordic iperf3 integration as :ref:`dect_shell_application` (``iperf3`` shell command, ``CONFIG_NRF_IPERF3``).
:file:`iperf3-common.conf` enables **TCP** and **IPv6-only** (``CONFIG_NET_IPV4=n`` to fit RAM on nRF9151 with RPC) and turns off DNS/mDNS. Add **either** :file:`iperf3-tx.conf` (iperf client) **or** :file:`iperf3-rx.conf` (iperf server). These fragments keep **256-byte** ``CONFIG_NET_BUF_DATA_SIZE`` (RAM-friendly on nRF9151) and **deeper** TX/RX pool counts so large ``-l`` UDP (many frags per datagram) does not block forever: on Zephyr, **UDP sockets stay blocking**, and each ``send()`` needs enough net_bufs while the RPC tunnel drains; undersized pools look like a **silent hang** (no new ``sent`` lines).

.. code-block:: console

   nrf/samples/dect/dect_rpc_client:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="iperf3-common.conf;iperf3-tx.conf" && west flash

.. code-block:: console

   nrf/samples/dect/dect_rpc_client:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="iperf3-common.conf;iperf3-rx.conf" && west flash

Run ``iperf3 --manual`` on the device for options. For traffic **over DECT** via the RPC tunnel, bring up the RPC iface (default ``rpc-dect``; ``CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME``), associate (``dect run connect``), then use the same IPv6 bind/connect examples as in the **iperf3 support** subsection of :ref:`dect_shell_application`.

Testing
=======

|test_sample|

Two-board setup
===============

#. Complete wiring in :ref:`dect_rpc_client_uart_wiring` between client and server.
#. Program the server (:ref:`dect_rpc_server`) and this client to two kits.
#. |connect_kit| for each kit and |connect_terminal| on each (**115200** 8N1).
#. Follow :ref:`dect_rpc_client_rpc_init`: **client first**, then **server**.
#. On the client, confirm ``RPC UART test: OK`` and run ``dect status`` when the interface is up.

Serial terminal
***************

Shell and LOG output use the **console UART** at **115200** baud, 8N1.

* **nRF9151 DK:** J-Link CDC ACM on the kit USB; console is UART0, RPC is UART1.

.. _dect_rpc_client_uart_wiring:

UART wiring (RPC link)
**********************

Baud rate is **1000000 (1 Mbps)** with **hardware flow control** (RTS/CTS): **four signals plus GND**.
**Connect by signal (TX↔RX), not by pin label alone** when wiring kits.

The default board devicetree uses the **same UART1 pins** as the **Serial Modem (SM)** DK board file in :ref:`dect_shell_application` (:file:`dect_shell/boards/nrf9151dk_nrf9151_ns_sm.overlay`): **P0.11** TX, **P0.10** RX, **P0.13** RTS, **P0.12** CTS.
That lets you reuse the same fly-leads when switching between **dect_shell** ``FILE_SUFFIX=sm`` builds and **dect_rpc_*** builds (this sample does **not** enable the SLM modem on UART1—only nRF RPC).

Two nRF9151 DKs (client and server)
===================================

.. table:: Wire two nRF9151 DKs as RPC client and RPC server.

   +---------------------------+---------------------------+
   | RPC client                | RPC server                |
   +---------------------------+---------------------------+
   | nRF9151 DK                | nRF9151 DK                |
   |                           |                           |
   +===========================+===========================+
   | **P0.11** (TX)            | **P0.10** (RX)            |
   +---------------------------+---------------------------+
   | **P0.10** (RX)            | **P0.11** (TX)            |
   +---------------------------+---------------------------+
   | **P0.13** (RTS)           | **P0.12** (CTS)           |
   +---------------------------+---------------------------+
   | **P0.12** (CTS)           | **P0.13** (RTS)           |
   +---------------------------+---------------------------+
   | GND                       | GND                       |
   +---------------------------+---------------------------+

Use **all signal rows plus GND** for reliable flow control.
These pins match the **Arduino header** UART1 routing on the nRF9151 DK used for Serial Modem in :file:`dect_shell/boards/nrf9151dk_nrf9151_ns_sm.overlay`; see the `nRF9151 DK <https://www.nordicsemi.com/Products/Development-hardware/nRF9151-DK>`_ user guide and schematic.

If you need RPC on the **legacy** UART1 pins (**P0.28** / **P0.29** / **P0.16** / **P0.17**), add a custom board devicetree file that removes ``uart1_default_alt`` / ``uart1_sleep_alt`` and restores the default SoC pinctrl for ``&uart1``.

If the server uses UART1 for something else (for example a different firmware image), use a server board devicetree change that moves RPC to **UART0** and wire this client's RPC UART to that UART (see :ref:`dect_rpc_server`).

.. _dect_rpc_client_rpc_init:

RPC initialization
********************

RPC is initialized in **main()** on both sides (no shell command).
The **client blocks** in ``nrf_rpc_init()`` until the **server** sends the group init after modem init.

**Order (required):**

1. **Start the client first** — wait for the log line that says the client is waiting for the server.
2. **Then start the server** — after modem init it sends the group init; the client unblocks.

If the server starts first, it fails after about 15 seconds.
Reset both kits and repeat with the **client** first.

RPC UART test (boot and shell)
******************************

After init, the client runs one **ping** over RPC at boot (``RPC UART test: OK`` or timeout exit).
Use ``dect rpc ping`` anytime to re-test.

Troubleshooting
***************

If you see ``nrf_rpc_uart: Ack timeout``, ``RPC send failed``, or on the server ``nrf_rpc_init failed: -35`` / ``Failed to send group init packet ... err: -71``, the RPC initialization order was wrong or the UART path is not exchanging HDLC ACKs.

**Most common cause:** the server was started before the client.

**Checklist:**

1. **Init order:** Client waiting log first, then boot server until both log ``RPC initialized``.
2. **First PING Ack timeout:** Tune ``CONFIG_NRF_RPC_THREAD_PRIORITY`` / pool sizes in ``prj.conf`` if the RPC thread pool cannot keep up with UART + DECT load.
3. **9151–9151 wiring:** TX↔RX and RTS↔CTS crossed, plus GND; verify continuity (default pins: **P0.10–P0.13**, same as dect_shell SM DK board wiring).
4. **Baud / UART:** ``current-speed = <1000000>``, explicit ``pinctrl`` for ``&uart1``, and ``nordic,rpc-uart = &uart1`` on both default builds.
5. **Debug:** ``CONFIG_NRF_RPC_TR_LOG_LEVEL_DBG=y`` and ``CONFIG_NRF_RPC_LOG_LEVEL_DBG=y`` in both ``prj.conf``, rebuild.
6. **ACK timing (reliable UART RPC only):** In :file:`rpc-uart-reliable.conf`, increase ``CONFIG_NRF_RPC_UART_ACK_WAITING_TIME`` / ``CONFIG_NRF_RPC_UART_TX_ATTEMPTS`` if the link is marginal.
7. **GET_ADDRS / ``dect status`` timeout after ping OK:** On the server try ``CONFIG_NRF_RPC_UART_RX_POLL_FALLBACK=y`` (and RX thread priority tuning), TR debug on the server, longer post-ping delay in client ``main.c``, or (if using reliable UART RPC) higher ACK wait/attempts.

**UART RPC mode:** The default is **non-reliable** (``CONFIG_NRF_RPC_UART_RELIABLE=n``) for throughput.
Use :ref:`dect_rpc_client_extra_rpc_uart_reliable` on **both** kits for per-frame ACKs on marginal wiring.
``CONFIG_DECT_NR_RPC_SERVER_EVT_PACING_MS`` defaults to **3** ms (non-reliable) / **20** ms (reliable) for **shell lines** and **control** events; **IF_RECEIVE** (bulk IP) is **not** paced when non-reliable so iperf/UDP is not capped at a few hundred packets per second.

mDNS
****

With :ref:`dect_rpc_client_extra_mdns`, the client advertises **dect-rpc-client.local** and can query other ``*.local`` names on the DECT side through the tunnel. The :ref:`dect_rpc_server` default image does not run mDNS (see :ref:`dect_rpc_server_mdns`).

Debugging (net stack traces)
****************************

See :ref:`dect_rpc_client_netdbg` in :ref:`dect_rpc_client_building`.

**RPC packet logging** (debug): per-packet lines are ``LOG_DBG`` — enable
``CONFIG_NET_DECT_RPC_LOG_LEVEL_DBG=y`` (and ``CONFIG_LOG_DBG=y``) to see
``RPC recv/send`` on the client and ``DECT recv`` / ``RPC recv -> DECT send`` on the server.

Dependencies
************

This sample uses the following Zephyr libraries:

* :ref:`zephyr:networking_api` — IP networking on the RPC client.

It uses nRF RPC over UART (see :file:`prj.conf` and the sample :file:`Kconfig`).

Run it together with :ref:`dect_rpc_server` on a second nRF9151 DK (see :ref:`dect_rpc_client_uart_wiring`).
