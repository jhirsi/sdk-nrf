DECT NR+ RPC Unit Tests
=======================

Ztest unit tests for subsys/net/dect/rpc/{client,server,common} on native_sim.
Three test apps (client/, client_hooks/, server/); client and server never talk to each
other in-process.
client/prj.conf and server/prj.conf follow samples/dect/dect_rpc/prj.conf and server.conf
(mock transport and test shell backends excepted).
Coverage target: >=75% lines on */nrf/subsys/net/dect/rpc/* (merged Twister -C).
common/mock_dect_rpc_transport.c replaces the UART link (not nrfxlib/nrf_rpc).

Architecture - What Is Tested vs Mocked
=======================================

  +------------------+     +------------------+
  |  Client ztest    |     |  Server ztest     |
  |  shell / API     |     |  mock RX cmd      |
  |  mock RX evt     |     |  fixture L2/HAL   |
  +--------+---------+     +---------+---------+
           |                         |
           v                         v
  +--------+-----------------------------------+
  |  subsys/net/dect/rpc (client|server|common) |
  +--------+-----------------------------------+
           |
           v
  +--------+---------+
  | mock_dect_rpc_tr |
  +------------------+

Tested:
- DECT NR+ RPC client and server code under subsys/net/dect/rpc/.
- Server: real l2_dect; mock DECT MDM HAL (server/src/dect_mdm_mock.c).

Mocked:
- nRF RPC transport (common/mock_dect_rpc_transport.c; stock mock stays in tests/mocks/nrf_rpc).
- Client: no modem/L2. Server: no libmodem / nRF91 MAC.

Wire helpers: common/test_rpc_env.h, mock_dect_rpc_tr_expect_add() /
mock_dect_rpc_tr_receive(). IF_STATUS codec: common/status_codec_helpers.* and
client|server/src/status_codec_suite.c (roundtrip + field coverage on struct dect_status_info).
int32 response codec: client|server/src/int32_rsp_codec_suite.c
(dect_rpc_client|server_int32_rsp_codec; CONNECT/DISCONNECT/IF_SEND rsp).
Server async evts: expect_add + k_msleep (often no expect_done).

Twister scenarios
=================

  Scenario                         | Directory      | Notes
  ---------------------------------|----------------|-------------------------------
  net.dect.rpc.client              | client/        | default client prj.conf
  net.dect.rpc.client.auto_sync    | client_hooks/  | CONFIG_DECT_NR_RPC_AUTO_SYNC
  net.dect.rpc.server              | server/        | prj.conf (shell on)
  net.dect.rpc.server.no_shell     | server/        | shell_disabled.conf overlay

Server variants share the same sources; default prj.conf aligns with
samples/dect/dect_rpc/server.conf (CONFIG_DECT_NR_RPC_SHELL=y).

Link-state tests use MOCK_DECT_RPC_EXPECT_ANY_SEND pads when CONFIG_DECT_NR_RPC_SHELL is on
(SHELL_LINE interleaves with IF_ADDRS_CHANGED). See rpc_test_link_state_expect.h and
common/mock_dect_rpc_transport.h (wildcard send, sent_count, larger expect queue via
CONFIG_MOCK_DECT_RPC_TR_MAX_EXPECTED_PKTS).


Building and running
====================

Twister
-------

  cd nrf/tests/subsys/net/dect/rpc

Run all DECT NR+ RPC ztests (four scenarios on native_sim):

  west twister -T . -p native_sim -O twister-out

Same set, explicit scenario names:

  west twister -T . -p native_sim \
    -s net.dect.rpc.client \
    -s net.dect.rpc.client.auto_sync \
    -s net.dect.rpc.server \
    -s net.dect.rpc.server.no_shell \
    -O twister-out-all

Individual scenarios:

  west twister -T . -p native_sim -s net.dect.rpc.client -O twister-out-client

  west twister -T . -p native_sim -s net.dect.rpc.client.auto_sync -O twister-out-hooks

  west twister -T . -p native_sim -s net.dect.rpc.server -O twister-out-server

  west twister -T . -p native_sim \
    -s net.dect.rpc.server.no_shell \
    -O twister-out-server-no-shell

Use platform native_sim (32-bit). native_sim/native/64 is not supported here (nRF RPC OS
assert on 64-bit atomics).

Direct build and run
--------------------

  cd nrf/tests/subsys/net/dect/rpc/client   # or server/ or client_hooks/
  west build -p -b native_sim .
  ./build/<client|server|client_hooks>/zephyr/zephyr.exe

Requirements: NCS, native_sim, CONFIG_NRF_RPC_RESPONSE_TIMEOUT set in prj.conf (avoid hang).

Measuring DECT NR+ RPC code coverage
====================================

  cd nrf/tests/subsys/net/dect/rpc

  west twister -T . -p native_sim \
    -C --coverage-tool lcov --coverage-formats html,lcov \
    --gcov-tool gcov --coverage-basedir "$(west topdir)" \
    -O twister-out-cov

  lcov --extract twister-out-cov/coverage.info "*/nrf/subsys/net/dect/rpc/*" \
    --output-file twister-out-cov/dect_rpc_only.info --rc lcov_branch_coverage=1

  genhtml twister-out-cov/dect_rpc_only.info \
    --output-directory twister-out-cov/dect_rpc_only_html \
    --branch-coverage --legend --prefix "$(west topdir)"

Report: twister-out-cov/dect_rpc_only_html/index.html

Adding a new test case
======================

1. Add *\_suite.c under client/src or server/src (picked up by CMake GLOB).
2. Build RPC expectations with test_rpc_env.h; inject peer traffic via mock_dect_rpc_tr_receive().
3. Client injected events: queue RPC_EVT_ACK after each RPC_EVT.
4. RPC_CMD/RPC_EVT macros use compound literals — call mock_dect_rpc_tr_expect_add() only as
   direct statements in the test (not from helpers/loops); see link_state_suite.c pattern.
5. Server: one booted dect0 per binary (server_dect_fixture.c); release association in teardown
   if the suite associates (if_send_suite.c).
6. Variable SHELL_LINE output: assert fixed lines (see shell_l2_suite.c) or keep shell off.

Suite files document individual tests; see client/src/ and server/src/.

Client library (subsys/net/dect/rpc/client/): dect_rpc_if.c (net_if + iface RPC),
dect_rpc_client_ping.c, dect_rpc_client_shell_line.c, dect_rpc_shell.c, dect_rpc_conn_mgr.c.

Server library (subsys/net/dect/rpc/server/): dect_rpc_if.c (IF_SEND/GET_ADDRS, evt queue),
dect_rpc_server_ping.c, dect_rpc_server_shell_line.c (CONFIG_DECT_NR_RPC_SHELL),
dect_rpc_server_shell.c.

Server forward_recv (server/src/forward_recv_suite.c)
=====================================================

Covers dect_rpc_server_forward_recv() (evt queue) and dect_rpc_evt_work_fn() IF_RECEIVE
CBOR, plus L2 delivery when an RPC client session is active (dect_net_l2_recv ->
dect_rpc_server_forward_recv).

Direct forward_recv tests build packets with net_pkt_write(); that advances the read
cursor, while L2-delivered packets start at offset 0. Helpers call net_pkt_cursor_init()
after write so net_pkt_read() in forward_recv sees the full payload.

The L2 test (test_l2_net_recv_forwards_to_rpc_client) calls net_if_l2()->recv with a
40-byte IPv6 frame, primes the session with dect_net_l2_rpc_client_set_connected(true),
and expects IF_RECEIVE on the mock transport. On NET_OK the L2 path unrefs the packet;
do not unref in the test. Async evt work: queue exact RPC_EVT expectations, k_msleep,
then mock_dect_rpc_tr_expect_done() (see forward_recv_suite.c).
