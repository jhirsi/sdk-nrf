Building  - DECT NR+ with modem mac
###################################

Common dect_shell configuration:

.. code-block:: console

   nrf9151dk:
   west build -p -b nrf9151dk/nrf9151/ns

FT/Sink: Border Router options
==============================

Preferred: LTE with SLM running on external 9151DK
--------------------------------------------------

* SLM running on external 9151DK:

.. code-block:: console

   nrf/applications/serial_lte_modem:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="overlay-ppp.conf;overlay-cmux.conf;overlay-zephyr-modem.conf" -DEXTRA_DTC_OVERLAY_FILE="overlay-external-mcu.overlay"


* DeSH with FT/Sink configuration using CELLULAR_MODEM Zephyr feature:

.. code-block:: console

   nrf/samples/dect/dect_mac/dect_shell:
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="overlay-nrf9151-slm.conf" -DEXTRA_DTC_OVERLAY_FILE="nrf9151-slm.overlay;hwfc_uart1.overlay"

* Wiring as in desh nrf9151-slm.overlay and in SLM overlay nrf/applications/serial_lte_modem/boards/nrf9151dk_nrf9151_ns.overlay:

.. code-block:: console

   nRF9151DK with desh -> nRF9151DK with SLM/ppp modem:
   10 (TX) -> 11 (RX)
   11 (RX) -> 10 (TX)
   12 (RTS) -> 13 (CTS)
   13 (CTS) -> 12 (RTS)

   20 (PWR) -> 31 (PWR)

NOTE: change VDD (nPM VOUT1) from 1.8V to 3.3V with “nRF Connect for Desktop → Board Configurator” in both DKes.

Ethernet with Phytec shield
---------------------------

NOTE 1: This config not used in a while....

NOTE 2: change VDD (nPM VOUT1) from 1.8V to 3.3V with “nRF Connect for Desktop → Board Configurator” before connecting the shield.

* DeSH with Phytec Shield support:

.. code-block:: console

   nrf9151dk:
   west build -p -b nrf9151dk/nrf9151/ns -- -DOVERLAY_CONFIG="overlay-phytec_eth_client.conf" -DSHIELD="link_board_eth"

Note: ethernet needs to offer IPv6 connectivity to the Internet and zephyr networking interface needs to have global ipv6 address, e.g. get from ipv6 RA.
MoSH with custom LTE-Eth GW functionalities have been used that.

Only with Ethernet sink:
mosh LTE-Eth GW:
Add route to PT device via FT/Sink device with Ethernet:
net route add <eth_iface_idx> <pt_global_ipv6addr> <eth_ll_addr_in_ft>

For example:
net route add 1 2001:14bb:af:151:0:29a:0:22b fe80::29a:0:22b

iperf3 support
==============

To build the DeSh sample with iperf3 support, for example:

PT (or FT without the sink) device: dect_shell with zephyr network management based shell commands:

.. code-block:: console

   nrf9151dk:
   west build -p -b nrf9151dk/nrf9151/ns

   With iperf3 support with TX optimized (usually acts as iperf3 client in PT device):
   west build -p -b nrf9151dk/nrf9151/ns -- -DOVERLAY_CONFIG="overlay-iperf3-common.conf;overlay-iperf3-tx.conf"

   With iperf3 support with RX optimized (usually acts as iperf3 server in FT device):
   west build -p -b nrf9151dk/nrf9151/ns -- -DOVERLAY_CONFIG="overlay-iperf3-common.conf;overlay-iperf3-rx.conf"

nRF Cloud
=========

Certificates and credentials
----------------------------

References:
https://nordicsemi.atlassian.net/browse/NCSDK-16759

Using nRF Cloud with hard-coded CA and device credentials:
https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/samples/cellular/nrf_cloud_multi_service/README.html#building_with_hard-coded_ca_and_device_credentials

Certs creation by using default approach with device uuid: see Readme-certs.txt for internal scripts.

.. important::
   Connecting to nRF Cloud using DECT NR+ currently requires that device credentials are used insecurely.

   The provided overlays for DECT NR+ connectivity use the :ref:`TLS Credentials Subsystem <zephyr:sockets_tls_credentials_subsys>` (with the :kconfig:option:`CONFIG_TLS_CREDENTIALS_BACKEND_VOLATILE`) to store credentials when not in use
   and settings subsystem to store certs in NVS without secure storage. Using of these is not recommended in production.
   :kconfig:option:`CONFIG_TLS_CREDENTIALS_BACKEND_PROTECTED_STORAGE` would be more secure but private key still has to be loaded into unprotected memory during TLS connections.

MQTT
----

PT device: dect_shell with zephyr network management based shell commands (default UUID approach supported):

.. code-block:: console

   nrf9151dk:
   $ west build -p -b nrf9151dk/nrf9151/ns -- -DOVERLAY_CONFIG="overlay-nrf_cloud_mqtt.conf"

COAP
----

PT device: dect_shell with zephyr network management based shell commands:

.. code-block:: console

   nrf9151dk:
   $ west build -p -b nrf9151dk/nrf9151/ns -- -DOVERLAY_CONFIG="overlay-nrf_cloud_coap.conf"

Note: system time is retrieved by using NTP.

REST (not supported)
--------------------

PT device: dect_shell with zephyr network management based shell commands:
.. code-block:: console

   nrf9151dk:
   $ west build -p -b nrf9151dk/nrf9151/ns -- -DOVERLAY_CONFIG="overlay-nrf_cloud_mqtt.conf;overlay-nrf_cloud_rest.conf"

Note: set local time:
date set 2025-04-01 14:15:00

Note: nrf cloud does not support ipv6 on REST, thus cloud_rest command does not work

Troubleshooting
***************

Enable more Zephyr side traces by enabling more CONFIG_LOG_XXXs in the application.

Modem traces can be enabled by providing a snippet with the west build command as shown in the following example for nRF9151 DK:

.. code-block:: console

   west build -p -b nrf9151dk/nrf9151/ns -- -DSNIPPET="nrf91-modem-trace-uart"

See :ref:`modem_trace` and :ref:`nrf91_modem_trace_uart_snippet` for more details.
