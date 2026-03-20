===========================================
DECT NR+ RPC: Networking interface over RPC
===========================================

Overview
********

This subtree implements a DECT NR+ networking path over **nRF RPC**: a **DECT NR+ RPC client** image
(without a DECT NR+ modem) exposes a Zephyr ``net_if`` that tunnels IPv6 to a **DECT NR+ RPC server** image
(with modem and ``l2_dect``) over **UART** (nRF RPC, HDLC-framed). The two images run on
separate boards linked by UART (RTS/CTS recommended).

- **DECT NR+ RPC client**: No DECT NR+ modem. All L2 send and receive for that ``net_if`` is forwarded
  over RPC as CBOR-framed payloads.
- **DECT NR+ RPC server**: Hosts the DECT NR+ modem and real DECT stack. Decodes RPC commands,
  injects TX packets into DECT NR+, and forwards RX toward the DECT NR+ RPC client.

On the DECT NR+ RPC client, the tunnel is a normal Zephyr ``net_if``: logical name from
``CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME`` when ``CONFIG_NET_INTERFACE_NAME``
is enabled; devicetree device ``dect_rpc``; L2 type ``DECT_RPC_L2``.

Architecture
************

High-level
==========

- **DECT NR+ RPC Client**: No DECT NR+ modem. Exposes a Zephyr ``net_if`` with L2 type ``DECT_RPC_L2``. TX goes out
  as RPC commands; RX is injected by RPC events on the DECT NR+ RPC client.
- **DECT NR+ RPC Server**: DECT NR+ modem and ``l2_dect``. Handles RPC from the DECT NR+ RPC client, injects/forwards packets
  on ``dect0``, and can run DECT NR+ L2 shell on behalf of the DECT NR+ RPC client.
- **Transport**: UART (nRF RPC over UART, HDLC). Unit tests may use ``CONFIG_MOCK_NRF_RPC_TRANSPORT``
  instead of UART (see ``nrf/tests/mocks/nrf_rpc``).

Control path (DECT NR+ RPC client → DECT NR+ RPC server, request/response)
===========================================================================

The DECT NR+ RPC client sends **commands**; the DECT NR+ RPC server decodes, runs the handler, and sends a **response**.

::

  DECT NR+ RPC CLIENT                                  UART                                  DECT NR+ RPC SERVER
  ──────────────────────────────────────────────────────────────────────────────────────────────────────────────
  Shell / app
      │
      ├─ IF_GET_ADDRS ────────────────────────────────────────────────► Decoder → read dect0 addrs/prefixes
      │     ◄──────────────────────────────────────────────  Response (addrs, MTU, carrier, dormant)
      │
      ├─ IF_STATUS ───────────────────────────────────────────────────► Decoder → DECT status (associations, fw)
      │     ◄──────────────────────────────────────────────  Response (status + associations)
      │
      ├─ SHELL (dect run <subcmd> [args]) ────────────────────────────► Decoder → dect_shell_exec_by_name()
      │     ◄──────────────────────────────────────────────  Response (0 or BUSY); output via SHELL_LINE
      │
      └─ PING ────────────────────────────────────────────────────────► Decoder → response 0
      │     ◄──────────────────────────────────────────────  Response (0)

Control path (server → client, events only)
============================================

The DECT NR+ RPC server sends **events** (no RPC response payload path for the DECT NR+ RPC client to ack as a command
response). DECT NR+ RPC client decoders finish with ``nrf_rpc_decoding_done()`` so the UART transport can
continue (including HDLC ACK when reliable UART RPC is enabled).

::

  SERVER                                    UART                    CLIENT
  ──────                                    ────                    ──────
  L2 / shell
      │
      ├─ IF_RECEIVE ─────────────────────────────────────────────► Decoder → net_pkt from payload → net_recv_data()
      │     (event; one RX IPv6 packet)
      │
      ├─ IF_LINK_STATE ─────────────────────────────────────────► Decoder → carrier/dormant → net_if_carrier_* / net_if_dormant_*
      │     (event; carrier + dormant bools)
      │
      ├─ IF_ADDRS_CHANGED ──────────────────────────────────────► Decoder → ``sync_addrs_from_server()`` (GET_ADDRS RPC)
      │     (event; notify client to re-sync addresses)
      │
      └─ SHELL_LINE ────────────────────────────────────────────► Decoder → shell_fprintf() / printk (one line of shell output)
            (event; one line of "dect run" output)

Data path (TX: client → server)
===============================

Application traffic on the client ``net_if`` → L2 send → CBOR encode of raw IPv6 bytes →
``IF_SEND`` → server decodes → ``net_if_send()`` on the server DECT interface → ``l2_dect`` → modem.

::

  CLIENT                                    UART                    SERVER
  ──────                                    ────                    ──────
  App → socket / conn_mgr
      │
      ▼
  net_if (dect_rpc L2)  ──► net_pkt
      │
      ▼
  dect_rpc_l2_send() ──► encode pkt bytes ──► IF_SEND (cmd_no_err) ──────────────► Decoder
                                                                                        │
                                                                                        ▼
                                                                                  net_pkt from buffer
                                                                                        │
                                                                                        ▼
                                                                                  net_if_send(server_dect_if, pkt)
                                                                                        │
                                                                                        ▼
                                                                                  l2_dect → modem → DECT air

Data path (RX: server → client)
===============================

Modem receives a frame → ``l2_dect`` receive path → optional RPC forward callback → clone,
queue, work sends ``IF_RECEIVE`` → client decoder builds ``net_pkt`` → ``net_recv_data()``.

::

  SERVER                                    UART                    CLIENT
  ──────                                    ────                    ──────
  DECT air ◄── modem
      │
      ▼
  dect_net_l2_recv(iface, pkt)
      │
      ├─ [CONFIG_DECT_NR_RPC_SERVER] dect_net_l2_rpc_forward_cb(iface, pkt)
      │       │
      │       ▼
      │   Clone pkt → enqueue to dect_rpc_evt_msgq → dect_rpc_evt_work
      │       │
      │       ▼
      │   Encode clone payload ──► IF_RECEIVE (event) ──────────────────────────► Decoder
      │                                                                              │
      │                                                                              ▼
      │                                                                        net_pkt_alloc(), write payload
      │                                                                              │
      │                                                                              ▼
      │                                                                        net_recv_data(iface, pkt)
      │                                                                              │
      │                                                                              ▼
      │                                                                        App / socket / conn_mgr
      │
      └─ (rest of L2 recv unchanged: routing, net_recv_data on server, etc.)

Block diagram (ASCII)
=====================

.. code-block:: text

   +------------------+     UART (nRF RPC + HDLC)     +------------------+
   |      CLIENT      |◄────────────────────────────►|      SERVER      |
   |  (no DECT modem) |                               | (DECT modem+L2)  |
   +--------+---------+                               +--------+---------+
            │                                                │
            │  Control (client → server)                     │
            │  IF_GET_ADDRS, IF_STATUS,                      │
            │  SHELL, PING                                   │
            │  ───────────────────────────────────────────► │
            │                                                │
            │  Control (server → client, events)             │
            │  IF_RECEIVE, IF_LINK_STATE,                    │
            │  IF_ADDRS_CHANGED, SHELL_LINE                  │
            │  ◄─────────────────────────────────────────── │
            │                                                │
            │  Data TX: IF_SEND (pkt bytes)                  │
            │  ───────────────────────────────────────────►│ net_if_send()
            │                                                │ → l2_dect → modem
            │  Data RX: IF_RECEIVE (pkt bytes)               │
            │  dect_net_l2_recv → forward_cb → event         │
            │  ◄────────────────────────────────────────────│
            │  net_recv_data()                               │
            ▼                                                ▼
   +------------------+                               +------------------+
   | net_if (dect_rpc)|                               | net_if (dect0)   |
   | L2 send/recv     |                               | l2_dect + driver |
   +------------------+                               +------------------+

Block diagram (Mermaid)
=======================

Source file: ``architecture.mmd``. To render:

1. Open https://mermaid.live
2. Paste the contents of ``architecture.mmd``
3. Export as PNG or SVG if needed

.. code-block:: mermaid

   %% Client net_if L2 is DECT_RPC; logical name is Kconfig (default ``rpc-dect``).
   flowchart LR
       subgraph CLIENT["CLIENT"]
           A[App]
           B[net_if dect_rpc]
           C[RPC client]
           A <--> B <--> C
       end

       UART[UART RPC+HDLC]

       subgraph SERVER["SERVER"]
           D[RPC server]
           E[net_if dect0]
           F[l2_dect]
           G[Modem]
           D --> E --> F --> G
       end

       C <--> UART <--> D

RPC IDs summary
===============

+------------------+----------------------+----------------------+---------------------------+
| Path             | Direction            | RPC ID               | Purpose                  |
+==================+======================+======================+===========================+
| Control          | Client → Server      | IF_ENABLE            | Server decoder only (stock client does not send) |
|                  |                      | IF_GET_ADDRS         | Sync addrs/prefixes/MTU   |
|                  |                      | IF_STATUS            | DECT status + associations|
|                  |                      | SHELL                | Run DECT shell command    |
|                  |                      | PING                 | UART/link test            |
+------------------+----------------------+----------------------+---------------------------+
| Control (events) | Server → Client      | IF_RECEIVE           | One RX IPv6 packet        |
|                  |                      | IF_LINK_STATE        | Carrier + dormant         |
|                  |                      | IF_ADDRS_CHANGED     | Notify re-sync addrs     |
|                  |                      | SHELL_LINE           | One line shell output     |
+------------------+----------------------+----------------------+---------------------------+
| Data TX          | Client → Server      | IF_SEND              | One IPv6 packet to send   |
+------------------+----------------------+----------------------+---------------------------+
| Data RX          | Server → Client      | IF_RECEIVE (event)   | One IPv6 packet received |
+------------------+----------------------+----------------------+---------------------------+

**Semantics (server-received, aside from the table):**

- ``DECT_RPC_CMD_IF_GET_ADDRS`` — client requests; server responds with IPv6 addresses,
  prefixes, MTU, carrier, and dormant so the client ``net_if`` mirrors ``dect0``.
- ``DECT_RPC_CMD_IF_STATUS`` — structured DECT status (modem, cluster, associations, FW).
- ``DECT_RPC_CMD_SHELL`` — run a DECT L2 shell line on the server; captured output streams
  as ``SHELL_LINE`` events; response is status only (**BUSY** if the server’s local shell owns DECT).

Data transfer (summary)
*************************

- Raw IPv6 packet bytes are carried as a CBOR byte string over RPC; both sides use ``net_pkt``.
  There is no extra encapsulation beyond CBOR framing for these commands.
- **Client L2 send**: encode ``net_pkt`` → ``DECT_RPC_CMD_IF_SEND``.
- **Client L2 recv**: ``DECT_RPC_CMD_IF_RECEIVE`` decoder allocates ``net_pkt``, writes payload,
  calls ``net_recv_data(iface, pkt)``.
- **Server IF_SEND**: decode buffer → build ``net_pkt``, set DECT iface and link-layer address from
  IPv6 destination (long_rd_id) → ``net_if_send(server_dect_if, pkt)``.
- **Server → client RX**: forward hook in DECT L2 (see below) clones RX traffic and emits
  ``DECT_RPC_CMD_IF_RECEIVE`` to the client.

Directory layout
****************

::

  nrf/subsys/net/dect/rpc/
  ├── CMakeLists.txt
  ├── Kconfig
  ├── README.rst
  ├── architecture.mmd
  ├── common/
  │   ├── CMakeLists.txt
  │   ├── dect_rpc_common.c
  │   ├── dect_rpc_common.h
  │   ├── dect_rpc_group.c
  │   └── dect_rpc_ids.h
  ├── client/
  │   ├── CMakeLists.txt
  │   ├── dect_rpc_client_net.h
  │   ├── dect_rpc_if.c
  │   └── dect_rpc_shell.c
  └── server/
      ├── CMakeLists.txt
      ├── dect_rpc_if.c
      └── dect_rpc_server_shell.c

Integration with existing DECT stack
************************************

- **Client**: Does **not** use ``l2_dect`` or the DECT modem driver. It only builds the
  RPC client and provides L2 ``DECT_RPC_L2`` so IPv6 is carried over RPC. No DECT
  associations or DECT net_mgmt on the client; the server owns the real stack.

- **Server** (``CONFIG_DECT_NR_RPC_SERVER``): Builds ``NET_L2_DECT`` and the modem driver.
  The RPC bridge in ``dect_rpc_if.c``:

  - **TX (``IF_SEND``)**: Decodes the CBOR payload, allocates a ``net_pkt`` on the server
    DECT interface resolved by ``net_if_get_by_name(CONFIG_DECT_MDM_DEVICE_NAME)`` (typically
    ``dect0``), then calls ``net_if_send_data()`` into ``l2_dect`` / modem.

  - **RX toward the RPC client** is implemented in ``l2_dect``, not in ``dect_mdm_rx``:

    - ``dect_net_l2_recv()`` (``nrf/subsys/net/l2_dect/dect_net_l2.c``), early in the IPv6
      path, checks ``rpc_client_connected`` (see ``dect_net_l2_rpc_client_set_connected()``,
      set from ``IF_GET_ADDRS`` / ``IF_SEND`` / ``IF_ENABLE`` handlers on the server) and a
      registered forward callback.
    - When both are set, it calls ``dect_net_l2_rpc_forward_cb(iface, pkt)`` — registered as
      ``dect_rpc_server_forward_recv()`` via ``dect_net_l2_rpc_forward_register()`` during
      ``dect_rpc_server_init()`` — then **``net_pkt_unref(pkt)``** and returns **``NET_OK``**,
      so the packet is **not** delivered further on the server’s IP stack (RPC-only path;
      no duplicate local ``net_recv_data()`` for that frame).
    - ``dect_rpc_server_forward_recv()`` copies the IPv6 payload with ``net_pkt_read()`` into
      a heap buffer, enqueues ``DECT_RPC_EVT_IF_RECEIVE`` on ``dect_rpc_evt_msgq``, and
      ``dect_rpc_evt_work`` sends ``DECT_RPC_CMD_IF_RECEIVE`` over RPC (with optional pacing
      from ``CONFIG_DECT_NR_RPC_SERVER_EVT_PACING_MS`` / reliable UART rules).

  - **Link and address sync**: ``dect_net_l2_link_state_register(dect_rpc_server_link_state_changed)``
    pushes carrier/dormant and address-change notifications to the client (events +
    optional re-sync), alongside a net_mgmt listener for IPv6 addr/prefix changes on the
    DECT interface.

Client iface: no ND, hostname and mDNS
**************************************

- **No NS/RS on client (same as native ``dect0``)**: The client’s ``dect_rpc`` net_if
  sets **``NET_IF_IPV6_NO_ND``** like ``dect_net_l2_init()`` for real DECT. There is no
  Router Solicitation (RS) nor Neighbor Solicitation (NS) on that iface. Addresses,
  prefixes and link state (carrier/dormant) are **synced from the server** via GET_ADDRS
  and IF_LINK_STATE over RPC, not from NDP.

- **MLD**: If **``CONFIG_NET_IPV6_MLD``** is enabled (Zephyr default when IPv6 is on),
  **``NET_IF_IPV6_NO_MLD`` is not set** — behavior matches native ``dect0`` (full
  ``net_ipv6_mld_join()`` / MLDv2 on the RPC tunnel as raw IPv6). If you disable
  ``CONFIG_NET_IPV6_MLD`` to save code, the driver sets **``NET_IF_IPV6_NO_MLD``**;
  then ``net_ipv6_mld_join()`` only adds the address unless ``dect_rpc_if.c`` applies
  the **logical** ``net_if_ipv6_maddr_join()`` for mDNS (``ff02::fb``).

- **Hostname**: Use **different** hostnames for client and server (e.g.
  ``dect-rpc-client`` and ``dect-rpc-server``). They are two separate devices; the
  identity visible on the DECT link is the server.

- **mDNS**: The **server** (DECT side) is on the radio link and **can** run an mDNS
  responder for the hostname that represents this device on the DECT network (if your
  application enables it). The server would answer mDNS queries from DECT peers. The
  client may run mDNS with its own hostname (e.g. for debugging or another network) but
  should **not** advertise the server’s hostname; only the server answers for the DECT
  network identity when a responder is enabled there.

DECT L2 shell over RPC
**********************

- **Optional** (``CONFIG_DECT_NR_RPC_SHELL`` on server, ``CONFIG_DECT_NR_RPC_CLIENT_SHELL`` on
  client): The **same** DECT L2 shell commands available on the server can be run from the
  **client** over RPC.

**How to run DECT L2 commands from the client**

On the client console, use:

  **dect run <subcmd> [args...]**

Examples (subcmd and args are the same as on the server’s local ``dect`` shell):

- ``dect run status`` – DECT status (modem, cluster, associations). Alternatively use
  ``dect status`` for a dedicated RPC that returns structured status.
- ``dect run nw_join`` – join network (same as ``dect nw_join`` on server).
- ``dect run nw_create`` – create network.
- ``dect run scan`` – scan (long-running; server blocks until done).
- Any other DECT L2 subcommand the server supports (see the server’s ``dect`` help).

RPC request is ``argc``, ``argv[0]..argv[argc-1]`` (subcmd + args). Response: ``status``
(0 OK, 1 BUSY/error), ``output`` (captured text). If the server’s **local** shell is
currently running a DECT command, the RPC handler returns **BUSY** (server shell always
wins). Long-running commands (e.g. scan) block the server until they finish; keep
output size within RPC buffer limits.

Kconfig
*******

- ``CONFIG_DECT_NR_RPC`` – Enables DECT NR+ over RPC (selects ``NRF_RPC``, ``NRF_RPC_CBOR``).
- ``CONFIG_DECT_NR_RPC_CLIENT`` – Client role (net_if on this core, no modem).
- ``CONFIG_DECT_NR_RPC_SERVER`` – Server role (has modem + ``l2_dect``, forwards packets).
- ``CONFIG_DECT_NR_RPC_NET_IF`` – Default y on client and server. Client: ``dect_rpc``
  net_if plus send/recv path. Server: RPC bridge in ``dect_rpc_if.c`` (IF_SEND,
  IF_GET_ADDRS, forwarding, etc.); must be y on the server or the client’s RPC
  commands are rejected as unknown.
- ``CONFIG_DECT_NR_RPC_SHELL`` – (Server only.) Run DECT L2 shell over RPC; selects
  ``DECT_L2_SHELL_LIB``. Client can send ``dect run <subcmd> [args...]``; server
  runs the same L2 shell and returns output or BUSY.
- ``CONFIG_DECT_NR_RPC_AUTO_SYNC`` – (Client, default y.) Run a single GET_ADDRS sync
  a few seconds after RPC init. On first success, the client brings the net_if up
  (``net_if_up``), so the interface effectively auto-starts after the first sync
  without the user running ``net iface up``. The iface still uses ``NET_IF_NO_AUTO_START``
  at boot so it does not come up before sync.
- ``CONFIG_DECT_NR_RPC_SERVER_EVT_PACING_MS`` – (Server.) Milliseconds to sleep after
  **IF_LINK_STATE** / **IF_ADDRS_CHANGED** RPC events, and after each **SHELL_LINE**
  when forwarding shell output. After **IF_RECEIVE** (bulk DECT→client), pacing runs
  only if ``CONFIG_NRF_RPC_UART_RELIABLE`` is enabled (default **20** ms then). With
  reliable off, IF_RECEIVE is unpaced here; default **3** ms still spaces shell lines.

CMake / subsys
***************

- ``nrf/subsys/net/CMakeLists.txt``: add
  ``add_subdirectory_ifdef(CONFIG_DECT_NR_RPC dect/rpc)``.
- ``dect/rpc/CMakeLists.txt``: add ``common``, and conditionally ``client`` and
  ``server`` based on ``CONFIG_DECT_NR_RPC_CLIENT`` and ``CONFIG_DECT_NR_RPC_SERVER``.

Implementation notes
********************

- **Server IF_SEND**: Decode the CBOR buffer, create ``net_pkt`` with
  ``net_pkt_rx_alloc_with_buffer()`` or equivalent for TX, set ``net_pkt_set_family(pkt, AF_INET6)``,
  write the raw IPv6 bytes, derive ``target_long_rd_id`` from the IPv6 destination
  (e.g. ``dect_utils_lib_long_rd_id_from_ipv6_addr()``), set
  ``net_pkt_lladdr_dst(pkt)`` from that, then call ``net_if_send(server_dect_if, pkt)``.
  The existing ``dect_net_l2_send()`` will then run and call the real driver’s ``send``.
- **Client**: ``NET_L2_INIT`` for ``DECT_RPC_L2`` (send/recv/enable/flags),
  ``NET_DEVICE_INIT`` with device name ``dect_rpc`` and L2 type ``DECT_RPC_L2``, and an
  ``IF_RECEIVE`` event decoder that pushes packets into ``net_recv_data``.
- **RPC group**: Defined in ``dect_rpc_group.c`` with group string ``"dect_rpc"`` and the
  chosen transport (UART via ``CONFIG_DECT_NR_RPC_UART_TRANSPORT``, or mock transport for tests).

Possible extensions
*******************

- **Connection manager on client (connect/disconnect via RPC)**
  **Feasibility**: Yes. Implement a custom connectivity binding on the client that
  attaches to the ``dect_rpc`` iface. On ``connect``, send an RPC (e.g.
  ``DECT_RPC_CMD_CONNECT``) to the server; the server runs the existing DECT
  net_mgmt connect (``NET_REQUEST_DECT_NETWORK_JOIN`` for PT or
  ``NET_REQUEST_DECT_NETWORK_CREATE`` for FT). On ``disconnect``, send
  ``DECT_RPC_CMD_DISCONNECT``; the server runs ``NET_REQUEST_DECT_NETWORK_UNJOIN``
  or ``NET_REQUEST_DECT_NETWORK_REMOVE``. **Do not forward DECT net_mgmt events**
  (e.g. ``NET_EVENT_DECT_NETWORK_STATUS``) over RPC; keep the RPC API to L4-level
  state only. The client binding derives connectivity from existing state: carrier
  and dormant from IF_LINK_STATE (and GET_ADDRS), and addresses/prefixes from
  GET_ADDRS. So the binding uses ``net_if_carrier_ok()``, ``net_if_is_dormant()``,
  and the synced addresses to implement ``get_status`` and drive conn_mgr. When
  there is **no global address** (server has only link-local), GET_ADDRS still
  syncs that; the client sees the same addresses and can report link-only / no
  global connectivity without any extra RPC or DECT events.
  **Benefit**: Applications on the client use the standard conn_mgr API
  (``conn_mgr_if_connect()`` / ``conn_mgr_if_disconnect()``) without knowing
  that the modem lives on the server; one place to trigger and observe DECT
  connect/disconnect.

- **Neighbor list with infos on client**
  **Feasibility**: Yes. The server already has ``NET_REQUEST_DECT_STATUS_INFO_GET``
  (synchronous ``dect_status_info`` with ``parent_count``, ``parent_associations[]``,
  ``child_count``, ``child_associations[]``: long_rd_id, local/global IPv6). Add an
  RPC (e.g. ``DECT_RPC_CMD_GET_NEIGHBORS`` or encode status/neighbor snapshot in a
  response). Server fills the structure (using ``dect_net_l2_status_info_fill_association_data``
  and driver status_info_get), encodes it (e.g. CBOR), sends to client. Client
  decodes and can expose it via a small API or net_mgmt-style events.
  **Benefit**: Apps on the client can list DECT neighbors (parent/children, long_rd_id,
  IPv6 addresses, role) for UI, diagnostics, or routing without having the modem;
  the server remains the single source of truth for the DECT topology.
