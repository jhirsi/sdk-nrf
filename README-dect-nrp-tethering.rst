DECT NRP Tethering — Build Commands
=====================================

This document lists ``west build`` commands for building the DECT NR+ tethering
samples on the ``nrf9151dk/nrf9151/ns`` target with W5500 Ethernet shields.
Two shield variants are covered: **Seeed W5500** and **Arceli W5500**.

Seeed W5500
-----------

FT / Ethernet sink
~~~~~~~~~~~~~~~~~~

Run from ``samples/dect/dect_shell``:

.. code-block:: console

   west build -p -b nrf9151dk/nrf9151/ns -- \
     -DSHIELD=seeed_w5500 \
     -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf;dect_rx_pool.conf;dlc_resilient.conf" \
     -DDTC_OVERLAY_FILE=w5500-seeed-static-mac.overlay

PT & PC tethering
~~~~~~~~~~~~~~~~~

Run from ``samples/dect/dect_tether_ipv6``:

.. code-block:: console

   west build -p -b nrf9151dk/nrf9151/ns -- \
     -DSHIELD=seeed_w5500 \
     -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf;eth_w5500_10bt_hd.conf;dect_rx_pool.conf;dlc_resilient.conf" \
     -DDTC_OVERLAY_FILE=w5500-seeed-static-mac.overlay

Arceli W5500
------------

FT / Ethernet sink
~~~~~~~~~~~~~~~~~~

Run from ``samples/dect/dect_shell``:

.. code-block:: console

   west build -p -b nrf9151dk/nrf9151/ns -- \
     -DSHIELD=arceli_eth_w5500 \
     -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;dect_rx_pool.conf;dlc_resilient.conf" \
     -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

PT & PC tethering
~~~~~~~~~~~~~~~~~

Run from ``samples/dect/dect_tether_ipv6``:

.. code-block:: console

   west build -p -b nrf9151dk/nrf9151/ns -- \
     -DSHIELD=arceli_eth_w5500 \
     -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;dect_rx_pool.conf;dlc_resilient.conf" \
     -DDTC_OVERLAY_FILE=w5500-static-mac.overlay
