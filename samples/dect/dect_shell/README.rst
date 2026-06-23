.. _dect_shell_application:

nRF91x1: DECT NR+ Shell
#######################

.. contents::
   :local:
   :depth: 2

The DECT NR+ Shell (DeSh) sample application demonstrates how to set up a DECT NR+ application on top of the DECT NR+ networking stack and enables you to test various stack and modem features.

Requirements
************

The sample supports the following development kit and requires at least two kits:

.. table-from-sample-yaml::

.. include:: /includes/tfm.txt

Overview
********

DeSh enables testing of the DECT NR+ networking stack in the |NCS| with DECT NR+ modem firmware v2.x.

The subsections list the DeSh features, show shell command examples, and describe their usage.

The default :file:`prj.conf` enables unicast DNS only. Merge :file:`mdns-common.conf` for the Zephyr **mDNS / DNS-SD** stack (resolver and responder), RFC 6762 hop-limit handling, and DNS dispatcher tuning (``CONFIG_ZVFS_POLL_MAX``, buffer pools). Sample-specific mDNS options and pool tuning are in :file:`mdns-discover.conf`; see :ref:`dect_shell_mdns_discover_build`.

The **sample** options ``CONFIG_DECT_SHELL_MDNS_DNS_SD_ADVERTISE`` and ``CONFIG_DECT_SHELL_MDNS_DISCOVER`` default to off. Build with :file:`mdns-common.conf` and :file:`mdns-discover.conf` to enable DNS-SD advertisement on dect0 (stub UDP bind) and the ``dect discover`` shell command. Implementation lives under :file:`src/mdns/` in the sample tree.

Those fragments layer on :file:`prj.conf`, which assumes ``CONFIG_NET_NATIVE=y`` and ``CONFIG_NET_IPV6=y`` (IPv4 off).

The following abbreviations from the DECT NR+ MAC specification (`ETSI TS 103 636-4`_) are used in the examples:

* FT: Fixed Termination point
* PT: Portable Termination point
* BR: Border Router that connects the DECT NR network to the Internet

.. note::
   For using this sample as a **channel access testing** tool in line with ETSI EN 301 406-2
   (DECT-2020 NR Part 2), see :ref:`dect_shell_channel_access_testing`.

Main command structure:

  .. code-block:: console

     at
       at_cmd_mode
     auto_connect
       enable
       disable
       sett_read
     cloud
       connect
       disconnect
       raw_data_tx (with CoAP)
     dect
       activate
       deactivate
       sett
       rssi_scan
       status
       scan
       associate
       dissociate
       cluster_start
       cluster_info
       neighbor_list
       neighbor_info
       nw_beacon_start
       nw_beacon_stop
       nw_create
       nw_remove
       nw_join
       nw_unjoin
       connect
       disconnect
       rx
       tx
       discover (mdns-discover.conf)
     hostname
       read
       write
     ping
     print
       timestamps
       cloud (with MQTT)
     version

Quick start tutorial
====================

**Step 1: Basic Two-Device Setup**

Device 1 (FT - Network Creator)::

   dect sett --dev_type FT
   dect activate
   dect connect
   # Wait for "Network status: created" message

Device 2 (PT - Network Joiner)::

   dect sett --dev_type PT
   dect activate
   dect connect
   # Wait for "Network status: joined" message

**Step 2: Verify Connection**

Both devices::

   dect status                          # Shows associations with addressing information
   ping -d <neighbor_ipv6_address>      # Test connectivity

Write and read hostname
=======================

DeSh command ``hostname``.

To set and read the hostname of the DECT NR+ device, use the following commands:

  .. code-block:: console

     desh:~$ hostname write dect-ft-device
     desh:~$ hostname read

Discover DECT NR+ peers (mDNS)
==============================

DeSh command ``dect discover``.

Merge :file:`mdns-common.conf` and :file:`mdns-discover.conf` to enable the mDNS stack and this command.

.. _dect_shell_mdns_discover_build:

* From the sample directory (copy-paste each line):

  .. code-block:: console

     cd nrf/samples/dect/dect_shell
     west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="mdns-common.conf;mdns-discover.conf"

Use this command to list peers that advertise the DNS-SD service ``_dect-nr._udp`` on the DECT NR+ network.
When ``CONFIG_NET_L2_DECT_ULA`` has configured an on-link ULA prefix on ``dect0``, the table includes a **ula (on-link)** column: the same /96 layout the L2 stack uses (common /64 from the ULA prefix, 32-bit long RD id from the mDNS address, last 32 bits of IID from the link-local AAAA), so you can ping or connect over ULA even though mDNS answers are often link-local.

* Usage example:

  .. code-block:: console

      desh:~$ dect discover

      dect discover: mDNS PTR _dect-nr._udp, then AAAA
      ula (on-link): from L2 ULA /96 + long_rd + LL tail ("-" if unavailable)
      discovery host: dect-nr+-device.local
      (PTR browse may take up to ~8 s per service.)
      _dect-nr._udp.local
      PTR pass done, 2 host(s) — resolving AAAA
      AAAA 2 host(s):
      # | host                     | kind | ipv6 (mDNS)                     | ula (on-link)                   | long_rd_id
      ----------------------------------------------------------------------------------------------------------------------
      1 | dect-nr+pt1.local        | LL   | fe80::e64c:7945:1c99:a829       | fd12:3456:789a:bbcc:0000:0000:1c99:a829 | 479832105 (0x1c99a829)
      2 | dect-nr+pt2.local        | LL   | fe80::e64c:7945:dd71:19fe       | fd12:3456:789a:bbdd:0000:0000:dd71:19fe | 3715176958 (0xdd7119fe)

      dect discover: finished (2 host(s))

Application settings
====================

DeSh command ``dect sett``.

You can store some of the main DeSh command parameters into settings that are persistent between sessions.
The settings are stored in the persistent storage and loaded when the application starts.

Examples
--------

* See the usage and read the current settings:

  .. code-block:: console

     desh:~$ dect sett -h
     desh:~$ dect sett -r

* Reset the settings to their default values:

  .. code-block:: console

     desh:~$ dect sett --reset

* Change the default TX power for the cluster beacon:

  .. code-block:: console

     desh:~$ dect sett --cluster_max_beacon_tx_pwr 4

* Change the default band to ``2``:

  .. code-block:: console

     desh:~$ dect sett -b 2

Activate DECT NR+ stack
========================

DeSh command ``dect activate``.

* Activate DECT NR+ stack:

  .. code-block:: console

     desh:~$ dect activate

RSSI measurement
================

DeSh command ``dect rssi_scan``.

Execute RSSI measurement/scan.

* Execute shorter (100 frames on each channel) RSSI measurements on all channels on band #1:

  .. code-block:: console

     desh:~$ dect rssi_scan -b 1 --frames 100

FT: Start a cluster manually
============================

DeSh command ``dect cluster_start``.

The ``dect cluster_start`` command starts a DECT NR+ cluster based on settings.
The command is available only for FT devices.

Examples
--------

* Set device as an FT device and set the transmission ID:

  .. code-block:: console

     desh:~$ dect sett --dev_type FT -t 1

* Activate DECT NR+ stack (if not already activated by auto_activate setting):

  .. code-block:: console

     desh:~$ dect activate

* Start a DECT NR+ cluster as set in settings:

  .. code-block:: console

     desh:~$ dect cluster_start
     Cluster start initiated.
     NET_EVENT_DECT_RSSI_SCAN_RESULT
     RSSI scan result:
     Channel:                             1657
     All subslots free:                   yes
     Busy percentage:                     0%
     NET_EVENT_DECT_RSSI_SCAN_DONE: scan done
     NET_EVENT_DECT_CLUSTER_CREATED_RESULT
     Cluster started/reconfigured at channel 1657.

FT: Start advertising the created cluster by starting a periodic network beacon
===============================================================================

DeSh command ``dect nw_beacon_start``.

The ``dect nw_beacon_start`` command starts sending of a DECT NR+ network beacon.
The command is available only for FT devices.

* Start a DECT NR+ network beacon at channel 1659 with additional channels:

  .. code-block:: console

     desh:~$ dect nw_beacon_start -c 1659 --add_channels 1661,1663,1665
     ..
     NW beacon started.

FT: Creating a DECT NR+ network
===============================

DeSh command ``dect nw_create``.

The ``dect nw_create`` command creates a DECT NR+ network in a set band.
This higher level command combines the ``rssi_scan`` and ``cluster_start`` commands and creates a DECT NR+ network.
Optionally, it also starts a network beacon on a set channel (preset in settings using ``dect sett --nw_beacon_channel <channel>``), which means it includes functionalities of ``dect nw_beacon_start``.
The command is available only for FT devices.

* Create a DECT NR+ network in a set band (FT device is activated but no cluster running):

  .. code-block:: console

     desh:~$ dect sett --nw_beacon_channel 1659
     Settings updated.
     desh:~$ dect nw_create
     Network creation initiated.
     NET_EVENT_DECT_RSSI_SCAN_RESULT
     RSSI scan result:
     Channel:                             1657
     All subslots free:                   yes
     Busy percentage:                     0%
     NET_EVENT_DECT_RSSI_SCAN_DONE: scan done
     NET_EVENT_DECT_CLUSTER_CREATED_RESULT
     Cluster started/reconfigured at channel 1657.
     NET_EVENT_DECT_NW_BEACON_START_RESULT
     NW beacon started.
     FT: network created
     NET_EVENT_DECT_NETWORK_STATUS:
     Network status: created

PT: Manually scan for a DECT NR+ cluster
========================================

DeSh command ``dect scan``.

The ``dect scan`` command scans for DECT NR+ clusters and network beacons.

* Start a DECT NR+ scan on band #1 (PT device is activated but not associated with any cluster):

  .. code-block:: console

     desh:~$ dect scan -b 1
     Scan initiated.
     NET_EVENT_DECT_SCAN_RESULT
     Scan result:
      Beacon type:             Cluster
      Reception channel:       1657
      Long RD ID:              1 (0x00000001)
      NW ID:                   2271560481 (0x87654321)
      RX RSSI-2:               -32dBm
      RX SNR:                  26dB
      RX MCS index:            4
      RX Transmit power:       10 (10 dBm)
     NET_EVENT_DECT_SCAN_RESULT
     ...
     NET_EVENT_DECT_SCAN_RESULT
      Scan result:
      Beacon type:             NW
      Reception channel:       1659
      Long RD ID:              1 (0x00000001)
      NW ID:                   2271560481 (0x87654321)
      RX RSSI-2:               -31dBm
      RX SNR:                  26dB
      RX MCS index:            4
      RX Transmit power:       10 (10 dBm)
      Current cluster channel: not available
      Next cluster channel:    1657
      NET_EVENT_DECT_SCAN_RESULT
      Scan result:
      Beacon type:             NW
      Reception channel:       1659
      Long RD ID:              1 (0x00000001)
      NW ID:                   2271560481 (0x87654321)
      RX RSSI-2:               -31dBm
      RX SNR:                  26dB
      RX MCS index:            4
      RX Transmit power:       10 (10 dBm)
      Current cluster channel: not available
      Next cluster channel:    1657
     NET_EVENT_DECT_SCAN_DONE
     Scan request done

PT: Associate with an FT device
===============================

DeSh command ``dect associate``.

The ``dect associate`` command associates a PT device with an FT device.

* Associate with a scanned FT device:

  .. code-block:: console

     desh:~$ dect associate -t 1
     NET_EVENT_DECT_ASSOCIATION_CHANGED
      DECT_ASSOCIATION_CREATED:
       Association created with long RD ID:                 1
       Neighbor role:                                       Parent
     PT: Joined a network
     NET_EVENT_DECT_NETWORK_STATUS:
      Network status: joined

* See the DECT NR+ status:

  .. code-block:: console

     desh:~$ dect status
     DECT NR+ status:
      Modem FW version:             mfw-nr+_nrf91x1_2.0.0
      Modem activated:              yes
      Cluster running:              no
      Network beacon running:       no
      Associations:
         Parent long RD ID:              1 (0x00000001)
            Local IPv6 address:           fe80::1:0:1

* See the networking status of the DECT NR+ interface:

  .. code-block:: console

     desh:~$ net iface

PT: Joining a DECT NR+ network
===============================

DeSh command ``dect nw_join``.

This higher level command combines  the ``scan`` and ``associate`` commands and joins the found network in a set band.

* Join a DECT NR+ network (PT device is activated but not associated with any network):

  .. code-block:: console

     desh:~$ dect nw_join
     ...
     NET_EVENT_DECT_NETWORK_STATUS:
     Network status: joined

PT: ICMPv6 ping an FT device
============================

DeSh command ``ping``.

The ``ping`` command sends ICMPv6 echo request to an FT device using the AF_INET6/SOCK_RAW/IPPROTO_IP sockets.

* PT device: Using global IPv6 address of the FT device (the global address is only available if the FT device is connected to the Internet):

  .. code-block:: console

     desh:~$ ping -d 2001:14bb:119:35b5:0:1:0:1
     Initiating ping to: 2001:14bb:119:35b5:0:1:0:1
     Source IP addr: 2001:14bb:119:35b5:0:1:0:29a
     Destination IP addr: 2001:14bb:119:35b5:0:1:0:1
     Pinging 2001:14bb:119:35b5:0:1:0:1 results: time=1.218secs, payload sent: 0, payload received 0
     ...
     Packets: Sent = 4, Received = 4, Lost = 0 (0% loss)
     Approximate round trip times in milli-seconds:
       Minimum = 992ms, Maximum = 1218ms, Average = 1048ms
     Pinging DONE

* PT device: Using local ipv6 address by first using mDNS to query the address by name:

  .. code-block:: console

     desh:~$ net dns dect-ft-device.local AAAA
     Query for 'dect-ft-device.local' sent.
     dns: fe80::1:0:1
     dns: All results received

     desh:~$ ping -d fe80::1:0:1

* PT device: Using a hostname of the FT device directly:

  .. code-block:: console

     desh:~$ ping -d dect-ft-device.local
     Initiating ping to: dect-ft-device.local
     Source IP addr: fe80::1:0:29a
     Destination IP addr: fe80::1:0:1
     Pinging dect-ft-device.local results: time=1.995secs, payload sent: 0, payload received 0
     ...
     Ping statistics for dect-ft-device.local:
        Packets: Sent = 4, Received = 4, Lost = 0 (0% loss)
     Approximate round trip times in milli-seconds:
        Minimum = 991ms, Maximum = 1995ms, Average = 1242ms
     Pinging DONE

* PT device: Zephyr IP stack ``net ping`` command is also available:

  .. code-block:: console

     desh:~$ net ping fe80::1:0:1

FT: Start RX for receiving raw data
===================================

* Start RX:

  .. code-block:: console

     desh:~$ dect rx start

PT: Send raw data to FT device
==============================

* Send data to a FT device:

  .. code-block:: console

     desh:~$ dect tx -t 1 -d "Hello FT device"

FT: Observe that data is received
=================================

* Observe received data:

  .. code-block:: console

     desh:~$
     Received data (len 16, src long RD ID 4257231875):
       Hello FT device

PT: Release association
=======================

DeSh command ``dect dissociate``.

The ``dect dissociate`` command releases the association between a PT device and an FT device.
You can also use the ``dect nw_unjoin`` command  to release the association and leave the DECT NR+ network.
This does not need the long RD ID of the FT device to be added as a parameter.

* Release the association with an FT device:

  .. code-block:: console

     desh:~$ dect dissociate -t 1

Using DECT NR+ Connection Manager
=================================

DeSh command ``dect connect``.

The ``dect connect`` command connects a DECT NR+ device to the DECT NR+ network.
This higher level command uses the DECT NR+ Connection Manager.
Depending on the configured device type, it can connect to a DECT NR+ cluster (PT device) or create a DECT NR+ network (FT device).

* FT device with Internet connection:

  .. code-block:: console

     desh:~$ dect sett -t 1 --dev_type FT
     desh:~$ dect connect
     connect initiated.
     NET_EVENT_DECT_RSSI_SCAN_RESULT
      RSSI scan result:
      Channel:                             1657
      All subslots free:                   yes
      Busy percentage:                     0%
     NET_EVENT_DECT_RSSI_SCAN_DONE: scan done
     NET_EVENT_DECT_CLUSTER_CREATED_RESULT
      Cluster started/reconfigured at channel 1657.
     NET_EVENT_DECT_NW_BEACON_START_RESULT
      NW beacon started.
     FT: network created
     NET_EVENT_DECT_NETWORK_STATUS:
     Network status: created

* PT device:

  .. code-block:: console

     desh:~$ dect sett -t 2 --dev_type PT
     desh:~$ dect connect
     NET_EVENT_DECT_ASSOCIATION_REQ_RESULT
     Association created with a parent with long RD ID 1
     PT: Joined a network
     NET_EVENT_DECT_NETWORK_STATUS:
     Network status: joined
     NET_EVENT_L4_CONNECTED: Network connectivity established and global IPv6 address assigned, iface 0x20010840
     NET_EVENT_L4_IPV6_CONNECTED: IPv6 connectivity established, iface 0x20010840

  .. note::
     This example output shows that the device is getting an Internet connection over the DECT NR+ network.

MQTT: Remote control using nRF Cloud
====================================

Once you have established an MQTT connection to nRF Cloud using the ``cloud`` command, you can use the **Terminal** window in the nRF Cloud portal to execute DeSh commands to the device.
This feature enables remote control of the DeSh application running on a device that is connected to cloud.
DeSh output, such as responses to commands and other notifications can be echoed to the ``messages`` endpoint and the **Terminal** window of the nRF Cloud portal.
Use the ``print cloud enable`` command to enable this behavior.
The data format of the input data in the **Terminal** window must be JSON.

Examples
--------

* PT device: Enabling printing also to cloud and establish the connection to nRF Cloud:

  .. code-block:: console

     desh:~$ print cloud enable

     desh:~$ cloud connect

* nRF Cloud: To request the DECT NR+ neighbor list for the PT device, enter the following command in the **Terminal** window of the nRF Cloud portal:

  .. code-block:: console

     {"appId":"DECT_SHELL", "data":"dect neighbor_list"}

  The response appears in the **d2c** terminal.

* nRF Cloud: To request icmpv6 ping towards Internet from a PT device, enter the following command in the **Terminal** window of the nRF Cloud portal:

  .. code-block:: console

     {"appId":"DECT_SHELL", "data":"ping -d nordicsemi.com"}

  The response appears in the **d2c** terminal.

Iperf3
======

DeSh command ``iperf``.

The ``iperf3`` command starts the iperf3 tool that is used for measuring data transfer performance both in uplink and downlink direction.

.. note::
   Run the iperf3 server on the FT device and the iperf3 client on the PT device (direct connection between the devices).
   Some features, for example, file operations and TCP option tuning, are not supported.

Examples
--------

* FT device: Create a network and see local IPv6 address for the DECT NR+ networking interface:

  .. code-block:: console

     desh:~$ dect sett -t 1 --dev_type FT

     desh:~$ dect connect
     ...
     NET_EVENT_DECT_CLUSTER_CREATED_RESULT
     Cluster started/reconfigured at channel 1659.
     FT: network created
     NET_EVENT_DECT_NETWORK_STATUS:
     Network status: created
     ...
     NET_EVENT_DECT_ASSOCIATION_CHANGED
     DECT_ASSOCIATION_CREATED:
      Association created with long RD ID:                 3872119375
      Neighbor role:                                       Child
     ...
     desh:~$ net iface 1
     Hostname: dect-ft-device
     Default interface: 1

     Interface dect0 (0x20017070) (<unknown type>) [1]
     =========================================
     Interface is down.
     Link addr : 00:00:00:01:00:00:00:01
     MTU       : 1280
     Flags     : AUTO_START,IPv6,NO_ND
     Device    : dect0 (0x57b2c)
     Status    : oper=DORMANT, admin=UP, carrier=ON
     IPv6 unicast addresses (max 2):
           fe80::1:0:1 autoconf preferred infinite
     IPv6 multicast addresses (max 3):
           ff02::fb  <not joined>
     IPv6 prefixes (max 2):
           <none>
     IPv6 hop limit           : 64
     IPv6 base reachable time : 30000
     IPv6 reachable time      : 23839
     IPv6 retransmit timer    : 0

* FT device: Start iperf3 server on the DECT NR+ networking interface on port 5555:

  .. code-block:: console

     desh:~$ iperf3 -s -B fe80::1:0:1 -p 5555 -1 -V -6

* PT device: Connect iperf3 client to the FT device iperf3 server on port 5555, using UDP protocol, with a payload size of 1220 bytes, for 50 seconds, and with a bandwidth of 2 Mbps:

  .. code-block:: console

     desh:~$ iperf3 -c fe80::1:0:1 -p 5555 -V -6 -u -l 1220 -t 50 -O 6 -b 1500k

User interface
==============

The buttons have the following functions:

Button 1:
   Raises a kill or abort signal.
   A long press of the button kills or aborts all supported running commands.
   You can abort commands ``iperf3`` and ``ping``.

Configuration
*************

|config|

Configuration options
=====================

Check and configure the following Kconfig options:

.. options-from-kconfig::
   :show-type:

Building
********

.. |sample path| replace:: :file:`samples/dect/dect_shell`

.. include:: /includes/build_and_run_ns.txt

See :ref:`cmake_options` for instructions on how to provide CMake options, for example to use a configuration overlay.

.. _dect_shell_ft_sink_border_router:

FT/Sink: Border Router
======================

This section describes how to build the DeSh sample for Internet access through a Border Router (BR) sink:

* LTE — cellular backhaul using `Serial Modem <ncs-serial-modem_>`_ on an external nRF9151 DK (FT/Sink build with ``FILE_SUFFIX=sm``).
* Ethernet — wired backhaul using a Zephyr W5500 shield on the same nRF9151 DK that runs the DECT NR+ sink (:ref:`arceli_eth_w5500` or :ref:`seeed_w5500`).

Both paths enable :kconfig:option:`CONFIG_NET_L2_DECT_BR` style border-router behavior; choose one backhaul per build (see notes under each variant).

LTE with `Serial Modem <ncs-serial-modem_>`_
--------------------------------------------

* `Serial Modem <ncs-serial-modem_>`_ :

  .. note::
     Serial Modem supports versions ``v1.0.0`` and ``696ca4525b423f8157f81b974525c2cd6c51cd3d``.

  .. code-block:: console

     west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="overlay-ppp.conf;overlay-cmux.conf" -DEXTRA_DTC_OVERLAY_FILE="overlay-external-mcu.overlay"

* DeSh with FT/Sink configuration using Zephyr's cellular modem feature:

  .. code-block:: console

     nrf/samples/dect/dect_shell:
     west build -p -b nrf9151dk/nrf9151/ns -- -DFILE_SUFFIX=sm

     west build -p -b thingy91x/nrf9151/ns -- -DPM_STATIC_YML_FILE="pm_static_thingy91x_nrf9151_ns.yml.manual" -DFILE_SUFFIX=sm
     
* Wiring as in the DeSh board overlay and `Serial Modem <ncs-serial-modem_>`_ :file:`ns-serial-modem/app/overlay-external-mcu.overlay` files:

  .. table:: Wire the boards together as shown.

     +------------------+-----------------+
     | Serial  Modem    | DECT NR+ Sink   |
     +------------------+-----------------+
     | nRF9151 DK       | nRF9151 DK      |
     |                  |                 |
     +==================+=================+
     | **P0.02** (TX)   | **P0.10** (RX)  |
     +------------------+-----------------+
     | **P0.03** (RX)   | **P0.11** (TX)  |
     +------------------+-----------------+
     | **P0.06** (RTS)  | **P0.12** (CTS) |
     +------------------+-----------------+
     | **P0.07** (CTS)  | **P0.13** (RTS) |
     +------------------+-----------------+
     | **P0.30** (RI)   | **P0.30** (RING)|
     +------------------+-----------------+
     | **P0.31** (DTR)  | **P0.31** (DTR) |
     +------------------+-----------------+
     | GND              | GND             |
     +------------------+-----------------+

  .. table:: Thingy:91 X wiring with Serial Modem.

     +------------------+-------------------+
     | Serial Modem     | DECT NR+ Sink     |
     +------------------+-------------------+
     | nRF9151 DK       | Thingy:91 X       |
     |                  |                   |
     +==================+===================+
     | **P0.02** (TX)   | **P0.21** (RX)    |
     +------------------+-------------------+
     | **P0.03** (RX)   | **P0.22** (TX)    |
     +------------------+-------------------+
     | **P0.06** (RTS)  | **P0.23** (CTS)   |
     +------------------+-------------------+
     | **P0.07** (CTS)  | **P0.24** (RTS)   |
     +------------------+-------------------+
     | **P0.30** (RI)   | **P0.25** (RING)  |
     +------------------+-------------------+
     | **P0.31** (DTR)  | **P0.19** (DTR)   |
     +------------------+-------------------+
     | GND              | GND               |
     +------------------+-------------------+

  .. note::
     On Thingy:91 X, using ``P0.19`` for DTR might require cutting SB9.

Ethernet with W5500 shield (Arceli)
-----------------------------------

Use this when the DECT sink (FT with BR) should reach the Internet over Ethernet.
The sample adds :file:`eth_common.conf` and :file:`eth_w5500.conf` (Kconfig) and an Arceli tuning overlay: default :file:`w5500-static-mac.overlay` (fixed locally administered Ethernet MAC), or :file:`w5500.overlay` for ``zephyr,random-mac-address`` (new MAC each boot).
For mDNS, append :file:`mdns-common.conf` and :file:`eth_mdns.conf` in that order (add :file:`mdns-discover.conf` before :file:`eth_mdns.conf` for ``dect discover``).
The pinout and SPI node come from the Zephyr shield devicetree: :file:`zephyr/boards/shields/arceli_eth_w5500/arceli_eth_w5500.overlay`.

.. note::
   Arduino **D8** (reset) and **D9** (interrupt) are shared with **BUTTON1** and **BUTTON2** on the nRF9151 DK.
   Thus, DK library is disabled in the Ethernet Kconfig overlays to avoid conflicts with Arceli shield.

* Build the DeSh sample as DECT BR sink over Ethernet (from the |NCS| workspace; default without mDNS):

  .. code-block:: console

     nrf/samples/dect/dect_shell:
     west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf" -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

   * With mDNS on dect0 and eth0:

     .. code-block:: console

        west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;mdns-common.conf;eth_mdns.conf" -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

   * Edit ``local-mac-address`` in :file:`w5500-static-mac.overlay` so each board on the same LAN has a unique MAC.

   * For a **random** Ethernet MAC each boot (``zephyr,random-mac-address``), use :file:`w5500.overlay` instead:

     .. code-block:: console

        west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf" -DDTC_OVERLAY_FILE=w5500.overlay

     With mDNS:

     .. code-block:: console

        west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;mdns-common.conf;eth_mdns.conf" -DDTC_OVERLAY_FILE=w5500.overlay

   * To also obtain an IPv6 address via DHCPv6 (in addition to SLAAC), append
     :file:`eth_dhcpv6_client.conf` to the configuration file list.  The DHCPv6
     client starts automatically when the Ethernet interface comes up, but only
     if SLAAC has not already provided a prefix:

     .. code-block:: console

        west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;eth_dhcpv6_client.conf" -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

   * Wiring as in the Arceli ETH W5500 shield overlay. Connect the RJ45 port to your LAN (router/switch).

  .. table:: nRF9151 DK + Arceli ETH W5500 (Arduino header).

      +-----------------------------+------------------------------------------+
      | W5500 / shield signal       | nRF9151 DK (Arduino / GPIO)              |
      +=============================+==========================================+
      | **SCS**                      | **D10** (**P0.10**)                     |
      +-----------------------------+------------------------------------------+
      | **MOSI**                    | **D11** (**P0.11**)                      |
      +-----------------------------+------------------------------------------+
      | **MISO**                    | **D12** (**P0.12**)                      |
      +-----------------------------+------------------------------------------+
      | **SCK/CLK**                 | **D13** (**P0.13**)                      |
      +-----------------------------+------------------------------------------+
      | **INT**                     | **D9** (**P0.09**)                       |
      +-----------------------------+------------------------------------------+
      | **RESET**                   | **D8** (**P0.08**)                       |
      +-----------------------------+------------------------------------------+
      | **3.3V**                    | Arduino **3.3V** or DK **VDD** (3.3 V)   |
      +-----------------------------+------------------------------------------+
      | **GND**                     | **GND**                                  |
      +-----------------------------+------------------------------------------+

Ethernet with W5500 shield (Seeed / Wiznet mapping)
-------------------------------------------------

WARNING: Wiznet w5500 shield (red one) is not working correctly and can burn your DK!

Use this when the DECT sink (FT with BR) should reach the Internet over Ethernet via the Zephyr :ref:`seeed_w5500` shield on the nRF9151 DK.
Merge :file:`eth_common.conf`, :file:`eth_w5500.conf` and :file:`eth_w5500_seeed.conf` for Ethernet sink without mDNS. For mDNS, append :file:`mdns-common.conf` and :file:`eth_mdns.conf` in that order (add :file:`mdns-discover.conf` before :file:`eth_mdns.conf` for ``dect discover``).
The Seeed shield (Rev 1.01) leaves the W5500 INTn disconnected, so :file:`eth_w5500_seeed.conf` enables :kconfig:option:`CONFIG_ETH_W5500_POLL_MODE` to service the driver over SPI instead of the interrupt line.
Devicetree comes from :file:`zephyr/boards/shields/seeed_w5500/seeed_w5500.overlay`
plus a sample overlay: default :file:`w5500-seeed-static-mac.overlay` (fixed locally administered Ethernet MAC), or :file:`w5500-seeed.overlay` for ``zephyr,random-mac-address`` (new MAC each boot).

.. note::
   The sample :file:`w5500.overlay` is Arceli-specific (targets ``&eth_w5500_arceli_eth_w5500``).
   For ``seeed_w5500``, use :file:`w5500-seeed-static-mac.overlay` or :file:`w5500-seeed.overlay` (targets ``&eth_w5500``).

* From the sample directory (default without mDNS):

  .. code-block:: console

     cd nrf/samples/dect/dect_shell
     west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf" -DDTC_OVERLAY_FILE=w5500-seeed-static-mac.overlay

   * With mDNS on dect0 and eth0:

     .. code-block:: console

        west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf;mdns-common.conf;eth_mdns.conf" -DDTC_OVERLAY_FILE=w5500-seeed-static-mac.overlay

   * Edit ``local-mac-address`` in :file:`w5500-seeed-static-mac.overlay` so each board on the same LAN has a unique MAC.

   * For a **random** Ethernet MAC each boot, use :file:`w5500-seeed.overlay` instead:

     .. code-block:: console

        west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf" -DDTC_OVERLAY_FILE=w5500-seeed.overlay

     With mDNS:

     .. code-block:: console

        west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf;mdns-common.conf;eth_mdns.conf" -DDTC_OVERLAY_FILE=w5500-seeed.overlay

Ethernet with ENC424J600 shield (Phytec link_board_eth)
-------------------------------------------------------

Use this when the DECT sink (FT with BR) should reach the Internet over Ethernet
via the Zephyr ``link_board_eth`` shield.

The sample adds :file:`eth_common.conf` and :file:`eth_link_board_eth.conf` (Kconfig).
For mDNS, append :file:`mdns-common.conf` and :file:`eth_mdns.conf` in that order (add :file:`mdns-discover.conf` before :file:`eth_mdns.conf` for ``dect discover``).
Pin mapping and SPI node come from the Zephyr shield devicetree:
:file:`zephyr/boards/shields/link_board_eth/link_board_eth.overlay`.

* Build the DeSh sample as DECT BR sink over Ethernet (from the |NCS| workspace; default without mDNS):

  .. code-block:: console

     nrf/samples/dect/dect_shell:
     west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=link_board_eth -DEXTRA_CONF_FILE="eth_common.conf;eth_link_board_eth.conf"

  * With mDNS on dect0 and eth0:

    .. code-block:: console

       west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=link_board_eth -DEXTRA_CONF_FILE="eth_common.conf;eth_link_board_eth.conf;mdns-common.conf;eth_mdns.conf"

  * To also obtain an IPv6 address via DHCPv6 (in addition to SLAAC), append
    :file:`eth_dhcpv6_client.conf`:

    .. code-block:: console

       west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=link_board_eth -DEXTRA_CONF_FILE="eth_common.conf;eth_link_board_eth.conf;eth_dhcpv6_client.conf"

  * Before attaching the shield to DK, set VDD (nPM VOUT1) to 3.3V.
  * Connect the shield, make sure that shield pins are not connected to any other pins on the DK and attach it to DK, then connect the RJ45 port to your LAN (router/switch).

.. _dect_shell_dect_rx_pool:

DECT-private RX pool (:file:`dect_rx_pool.conf`)
------------------------------------------------

When the DeSh sink (FT with BR) forwards heavy DECT uplink traffic to Ethernet, eth0 RX bursts allocate from the global Zephyr pools (``CONFIG_NET_PKT_RX_COUNT`` / ``CONFIG_NET_BUF_RX_COUNT``) and can **starve dect0 RX**, surfacing as ``RX packet allocation failed in ISR`` drops and follow-on ``nrf_modem_dect_dlc_data_tx returned NRF_ENOMEM`` warnings.

Append :file:`dect_rx_pool.conf` **last** in ``EXTRA_CONF_FILE`` to enable ``CONFIG_DECT_MDM_RX_PRIVATE_POOL`` — a DECT-only ``net_pkt`` slab and ``net_buf`` pool isolated from the global RX pools. Example with the Seeed W5500 shield:

.. code-block:: console

   cd nrf/samples/dect/dect_shell
   west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf;dect_rx_pool.conf" -DDTC_OVERLAY_FILE=w5500-seeed-static-mac.overlay

Runtime inspection (with ``CONFIG_NET_BUF_POOL_USAGE=y`` and ``CONFIG_MEM_SLAB_TRACE_MAX_UTILIZATION=y`` already set by the overlay):

.. code-block:: console

   desh:~$ dect_mdm rx_pool
   DECT private RX pool:
   Address         Total   Free    MaxUsed Name
   0x...           20      20      8       dect_mdm_rx_pkts (slab)
   0x...           40      40      8       dect_mdm_rx_bufs (bufs, 256 B)

Bump ``PRIVATE_PKT_COUNT`` first if ``MaxUsed == Total`` under sustained traffic; raise ``PRIVATE_BUF_COUNT`` if max-MTU uplink frames are dominant (each consumes ``ceil(1500 / BUF_SIZE)`` fragments).

.. _dect_shell_dlc_resilient:

Loss-resilient DLC profile (:file:`dlc_resilient.conf`)
-------------------------------------------------------

The modem's stock DLC defaults — ``LIFETIME_60_S`` and "release association on DLC discard" — are not a great match for a tether/sink deployment: stale SDUs can sit in the TX queue for up to 60 s, and a single discard-timer expiry tears down associations with child PTs and forces a reconnect storm. The :file:`dlc_resilient.conf` overlay replaces them with a loss-resilient profile:

* ``CONFIG_DECT_MDM_NRF_DLC_SDU_LIFETIME=23`` — ``LIFETIME_2_5_S``: applied to the FT's ``default_tx_flow_config[0]`` advertised at cluster start (the FT->PT direction). In the typical tether topology the PT sources data toward the internet, so **PT->FT is the heavy uplink** and **FT->PT mostly carries small TCP ACKs and control back to the PT**. 2.5 s (slightly more tolerant than the 1.5 s used on the PT side) is intentional: dropping a return-path ACK forces TCP to retransmit on the already-saturated uplink, which hurts throughput more than an ACK that's delivered late. For latency-sensitive workloads where stale SDUs are useless, drop the value at runtime to e.g. ``=4`` (10 ms) or ``=8`` (50 ms). The PT counterpart in :ref:`dect_tether_ipv6 <dect_tether_ipv6_dlc_resilient>` uses ``=21`` (1.5 s); the two sides do **not** need to use the same value.
* ``CONFIG_DECT_MDM_NRF_DLC_DISCARD_TIMER_RELEASE_ASSOC_COUNT=10`` — release the association with a child PT only after **10 consecutive** DLC-discard expiries (counter resets on successful DLC TX). With the 2.5 s lifetime above, a fully stuck **FT->PT** return path tolerates roughly **10 × 2.5 s ≈ 25 s** before reconnect. Use ``0`` to disable discard-driven release entirely.

Append :file:`dlc_resilient.conf` **last** in ``EXTRA_CONF_FILE``. Example with the Arceli W5500 ethernet sink:

.. code-block:: console

   cd nrf/samples/dect/dect_shell
   west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;dlc_resilient.conf" -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

Composable with :file:`dect_rx_pool.conf` (append both, in any order):

.. code-block:: console

   west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="eth_common.conf;eth_w5500.conf;dect_rx_pool.conf;dlc_resilient.conf" -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

Both knobs are also tunable at runtime via the DECT L2 shell, no rebuild required:

.. code-block:: console

   desh:~$ dect sett --dlc_sdu_lifetime 23
   desh:~$ dect sett --dlc_discard_release_assoc_count 10
   desh:~$ dect sett --read

The ``--read`` output annotates which TX flow the lifetime applies to — ``FT->PT`` here (``FT`` role advertises it in ``default_tx_flow_config[0]`` at cluster start). The matching PT-side overlay in :ref:`dect_tether_ipv6 <dect_tether_ipv6_dlc_resilient>` controls the **return path (PT->FT)** independently; pick each side's value for its own traffic profile.

Valid ``--dlc_sdu_lifetime`` values are ``1..31`` (0.5 ms .. 60 s, see ``enum dect_dlc_sdu_lifetime`` / ``nrf_modem_dect_dlc_sdu_lifetime``) or ``255`` for ``INFINITY``. Use ``31`` (60 s) to revert to the default at runtime.


iperf3 support
==============

To build the DeSh sample with iperf3 support (mDNS disabled in :file:`iperf3-common.conf`). From the sample directory:

.. code-block:: console

   cd nrf/samples/dect/dect_shell

   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="iperf3-common.conf;iperf3-tx.conf"

   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="iperf3-common.conf;iperf3-rx.conf"

nRF Cloud
=========

nRF Cloud offers location services and allows devices to report data to the cloud for collection and analysis.
This section describes how to build the DeSh sample with nRF Cloud support using MQTT and CoAP protocols.

Certificates
------------

You can store certificates on the device using a custom AT%CMNG command (implemented by DeSh custom AT command) in the following two ways:

* Directly with the AT command (``desh:~$ at at%cmng=******``) or the ``at_cmd_mode`` command (``desh:~$ at_cmd_mode start``), or by a script.
* Using the `Cellular Monitor app`_ to store the certificates to the modem (default nRF Cloud security tag).

  .. code-block:: console

     desh:~$ dect deactivate
     desh:~$ at at_cmd_mode start

.. note::
   As a result of the custom %CMNG command in DeSh, the MQTT credentials are stored insecurely in settings, but with CoAP, they are stored more securely in the Protected Storage.

MQTT
----

.. code-block:: console

   cd nrf/samples/dect/dect_shell
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="nrf_cloud_mqtt.conf"

CoAP
----

.. code-block:: console

   cd nrf/samples/dect/dect_shell
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="nrf_cloud_coap.conf"

   west build -p -b thingy91x/nrf9151/ns -- -DPM_STATIC_YML_FILE="pm_static_thingy91x_nrf9151_ns.yml.manual" -DEXTRA_CONF_FILE="nrf_cloud_coap.conf"

.. note::
   System time is retrieved by using NTP.
   For the CA certificate, only the nRF Cloud CoAP CA certificate needs to be stored on the device with CoAP.
   Do not store the Amazon root CA certificate on the device with CoAP due to crypto limitations for handling RSA certificates.
   Add ``-DFILE_SUFFIX=sm`` to enable FT/Sink Border Router over Serial Modem support.
   For Border Router over Ethernet instead, use the **Ethernet with W5500 shield** build (see :ref:`dect_shell_ft_sink_border_router`; do not mix with ``FILE_SUFFIX=sm``).

Dependencies
************

* DECT NR+ :ref:`Connection Manager <zephyr:conn_mgr_overview>` and related APIs:

  * .. doxygengroup:: dect_net_l2_mgmt
  * .. doxygengroup:: dect_net_l2

This sample uses the following |NCS| libraries:

* :ref:`dk_buttons_and_leds_readme`

In addition, it uses the following secure firmware component:

* :ref:`Trusted Firmware-M <ug_tfm>`
