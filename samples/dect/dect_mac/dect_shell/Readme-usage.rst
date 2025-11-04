.. _dect_shell_mdm_mac_application:

nRF91x1: DECT NR+ Shell for modem MAC
#####################################

.. contents::
   :local:
   :depth: 2

The DECT NR+ Shell (DeSh) for modem MAC sample application demonstrates how to set up a DECT NR+ application with the DECT NR+ modem MAC firmware and enables you to test various modem features.

.. important::

   The sample showcases the use of the :ref:`nrf_modem_dect_mac` interface of the :ref:`nrfxlib:nrf_modem`.

Requirements
************

The sample supports the following development kits and requires at least two kits:

.. table-from-sample-yaml::

.. include:: /includes/tfm.txt

Overview
********

DeSh enables testing of DECT NR+ networking stack with :ref:`nrf_modem_dect_mac` interface and
related modem features.
This sample is also a test application for aforementioned features.

The subsections list the DeSh features and show shell command examples for their usage.

.. note::
   To learn more about using a DeSh command, run the command without any parameters.

The following abbreviations from MAC specification are used in the examples:

* FT: Fixed Termination point
* PT: Portable Termination point
* BR: Border Router

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
       alert_tx (with CoAP)
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
       neigbor_list
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
     hostname
       read
       write
     ping
     print
       timestamps
       cloud (with MQTT)
     version

Quick Start Tutorial
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

   dect status                          # Should show associations
   ping -d <neighbor_ipv6_address>      # Test connectivity

Write and read hostname
======================
DeSh command: ``hostname``

You can set and read the hostname of the DECT NR+ device.
  .. code-block:: console

     desh:~$ hostname write dect-ft-device
     desh:~$ hostname read

Application settings
====================

DeSh command: ``dect sett``

You can store some of the main DeSh command parameters into settings that are persistent between sessions.
The settings are stored in the persistent storage and loaded when the application starts.

Examples
--------

* See the usage and read the current settings:

  .. code-block:: console

     desh:~$ dect sett -?
     desh:~$ dect sett -r

* Reset the settings to their default values:

  .. code-block:: console

     desh:~$ dect sett --reset

* Change the default TX power for the cluster beacon:

  .. code-block:: console

     desh:~$ dect sett --cluster_beacon_tx_pwr 4

* Change the maximum TX power:

  .. code-block:: console

     desh:~$ dect sett --tx_pwr -12

* Change the default band to ``2``:

  .. code-block:: console

     desh:~$ dect sett -b 2

Activate DECT NR+ stack
========================

DeSh command: ``dect activate``

* Activate DECT NR+ stack:

  .. code-block:: console

     desh:~$ dect activate

RSSI measurement
================

DeSh command: ``dect rssi_scan``

Execute RSSI measurement/scan.

* Execute shorter (100 frames on each channel) RSSI measurements on all channels on band #4:

  .. code-block:: console

     desh:~$ dect rssi_scan -b 4 --frames 100

* Stop RSSI measurement (TODO):

  .. code-block:: console

   desh:~$ dect rssi_scan stop

FT: Start a cluster manually
============================

DeSh commands:
   ``dect cluster_start``

The ``dect cluster_start`` command starts a DECT NR+ cluster based on settings.
The command is available only for FT devices.

Examples
--------
* Set device as a FT device and set unigue transmission ID:

  .. code-block:: console

      desh:~$ dect sett --dev_type FT -t 1

* Activate DECT NR+ stack (if not already activated by auto_activate setting):

  .. code-block:: console

     desh:~$ dect activate

* Start a DECT NR+ cluster as set in settings:

  .. code-block:: console

      desh:~$ dect cluster_start
      [00:33:00.175,415] <inf> DECT_NRP_MAC: Starting RSSI scanning for set band #1
      NET_EVENT_DECT_RSSI_SCAN_RESULT
      RSSI scan result:
      Channel:           1657
      All subslots free: yes
      NET_EVENT_DECT_RSSI_SCAN_DONE: scan done
      NET_EVENT_DECT_CLUSTER_CREATED_RESULT
      Cluster started at channel 1657.

FT: Start advertising a created cluster by starting a periodic network beacon
=============================================================================

DeSh command: ``dect nw_beacon_start``
The ``dect nw_beacon_start`` command starts sending of a DECT NR+ network beacon.
The command is available only for FT devices.

* Start a DECT NR+ network beacon at channel 1659 with additional channels:

  .. code-block:: console

     desh:~$ dect nw_beacon_start -c 1659 --add_channels 1661,1663,1665
     ..
     NW beacon started.

FT: Creating a DECT NR+ network
===============================

DeSh command: ``dect nw_create``
``dect nw_create`` command creates a DECT NR+ network to the set band.
This command is little bit higher level command that combines rssi_scan and cluster_start commands
and creates a DECT NR+ network in a set band. Optionally, also starts a network beacon, i.e. includes functionalities of``dect nw_beacon_start``.
The command is available only for FT devices.

* Create a DECT NR+ network in set band:

  .. code-block:: console

     desh:~$ dect nw_create

PT: Manually scan for a DECT NR+ cluster
========================================

DeSh command: ``dect scan``
The ``dect scan`` command scans for DECT NR+ clusters and network beacons.

* Start a DECT NR+ scan on band #1:

  .. code-block:: console

     desh:~$ dect scan -b 1
     ..
     Scan initiated.
      NET_EVENT_DECT_SCAN_RESULT
      Scan result:
         Beacon type:             Cluster
         Reception channel:       1657
         Long RD ID:              1 (0x00000001)
         NW ID:                   2271560481 (0x87654321)
         RX RSSI-2:               -53dBm
         RX SNR:                  25dB
         RX MCS index:            4
         RX Transmit power:       8
      NET_EVENT_DECT_SCAN_RESULT
      Scan result:
         Beacon type:             NW
         Reception channel:       1659
         Long RD ID:              1 (0x00000001)
         NW ID:                   2271560481 (0x87654321)
         RX RSSI-2:               -54dBm
         RX SNR:                  26dB
         RX MCS index:            4
         RX Transmit power:       8
         Current cluster channel: 0
         Next cluster channel:    1657
         Additional network beacon channel #1: 1661
         Additional network beacon channel #2: 1663
         Additional network beacon channel #3: 1665
      NET_EVENT_DECT_SCAN_DONE
      Scan request done

PT: Associate with a FT device
===============================

DeSh command: ``dect associate``
The ``dect associate`` command associates a PT device with a FT device.

* Associate with a scanned FT device:

  .. code-block:: console

      desh:~$ dect associate -t 1
      [00:01:19.260,620] <inf> DECT_NRP_MAC: dect_nrf91_driver_associate_req: association initiated
      NET_EVENT_DECT_ASSOCIATION_REQ_RESULT
      Association response from long RD ID 1: accepted
      NET_EVENT_DECT_PARENT_ASSOCIATION_CREATED
      Association created with a parent with long RD ID 1
      PT: Joined a network
      NET_EVENT_DECT_NETWORK_STATUS
      Network status: joined

* See the DECT NR+ status:

  .. code-block:: console

      desh:~$ dect status
      DECT NR+ status:
      Modem FW version:             mfwRD-nr+_nrf91x1_1.2.0-251.prealpha
      Modem activated:              yes
      Cluster running:              no
      Network beacon running:       no
      Associations:
         Parent long RD ID:              1 (0x00000001)
            Local IPv6 address:           fe80::1:0:1
            Global IPv6 address:          2001:14bb:119:35b5:0:1:0:1

* See networking status of DECT NR+ interface:

  .. code-block:: console

      desh:~$ net iface

PT: Joining a DECT NR+ network
===============================

DeSh command: ``dect nw_join``
The ``dect nw_join`` is little bit higher level command that combines scan and associate commands
and joins to found network in a set band.

* See the status:

  .. code-block:: console

      desh:~$ dect nw_join

FT: See the status
==================

* See the status:

  .. code-block:: console

      desh:~$ dect status
      DECT NR+ status:
      Modem FW version:             mfwRD-nr+_nrf91x1_1.2.0-251.prealpha
      Modem activated:              yes
      Cluster running:              yes
      Cluster channel:              1657
      Network beacon running:       yes
      Associations:
         Child #1 long RD ID:       666 (0x0000029a)
            Local IPv6 address:       fe80::1:0:29a
            Global IPv6 address:      2001:14bb:119:35b5:0:1:0:29a
      Border router and sink information:
         Border router network interface:             ppp0 (0x20014818)
         Border router global IPv6 address prefix/64: 2001:14bb:119:35b5::
            Connection to Internet should be available.

* See networking status of networking interface (if compiled with BR support, these should be also that networking interface printed):

  .. code-block:: console

     desh:~$ net iface


PT: ICMPv6 Ping a FT device
============================

DeSh command: ``ping``
The ``ping`` command sends ICMPv6 echo request to a FT device by using AF_INET6/SOCK_RAW/IPPROTO_IP sockets.

* PT device: by using global IPv6 address of the FT device (global address only available if FT device is connected to the internet):

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

* PT device: by using local ipv6 address by 1st using mDNS to query the address by name:

  .. code-block:: console

      desh:~$ net dns dect-ft-device.local AAAA
      Query for 'dect-ft-device.local' sent.
      dns: fe80::1:0:1
      dns: All results received

      desh:~$ ping -d fe80::1:0:1

* PT device: by using a hostname of the FT device directly:

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


FT: Start a RX for receiving raw data
=====================================

  .. code-block:: console

     desh:~$ dect rx start

PT: Send raw data to FT device
==============================

  .. code-block:: console

     desh:~$ dect tx -t 1 -d "Hello FT device"

FT: Observe that data is received
=================================

  .. code-block:: console

     desh:~$
     Received data (len 16, src long RD ID 1):
       Hello FT device

FT: Release association
=======================
DeSh command: ``dect dissociate``
The ``dect dissociate`` command releases the association between a PT device and a FT device.
The command is available only for FT devices.

* Release the association with a PT device:

  .. code-block:: console

     desh:~$ dect dissociate -t 1

Using DECT NR+ Connection Manager
=================================

DeSh command: ``dect connect``
The ``dect connect`` command connects a DECT NR+ device to the DECT NR+ network.
This is a higher level command that uses DECT NR+ Connection Manager.
Depending on set device type, it can connect to a DECT NR+ cluster (PT device) or create a DECT NR+ network (FT device).

* FT device with Internet connection:

  .. code-block:: console

      desh:~$ dect sett -t 1 --dev_type FT
      desh:~$ dect connect
      NET_EVENT_DECT_RSSI_SCAN_RESULT
      RSSI scan result:
      Channel:           1657
      All subslots free: yes
      NET_EVENT_DECT_RSSI_SCAN_DONE: scan done
      NET_EVENT_DECT_CLUSTER_CREATED_RESULT
      Cluster started at channel 1657.
      FT: network created
      NET_EVENT_DECT_NETWORK_STATUS
      Network status: created

* PT device:

  .. code-block:: console

      desh:~$ dect sett -t 2 --dev_type PT
      desh:~$ dect connect
      NET_EVENT_DECT_ASSOCIATION_REQ_RESULT
      Association created with a parent with long RD ID 1
      PT: Joined a network
      NET_EVENT_DECT_NETWORK_STATUS
      Network status: joined
      NET_EVENT_L4_CONNECTED: Network connectivity established and global IPv6 address assigned
      NET_EVENT_L4_IPV6_CONNECTED: IPv6 connectivity established

* FT device when 1st PT connected:

  .. code-block:: console

      NET_EVENT_DECT_CHILD_ASSOCIATION_CREATED
      Association created with a device with long RD ID 2
      NET_EVENT_L4_CONNECTED: Network connectivity established and global IPv6 address assigned
      NET_EVENT_L4_IPV6_CONNECTED: IPv6 connectivity established

MQTT: Remote control using nRF Cloud
====================================

Once you have established an MQTT connection to nRF Cloud using the ``cloud`` command, you can use the **Terminal** window in the nRF Cloud portal to execute DeSh commands to the device.
This feature enables remote control of the DeSh application running on a device that is connected to cloud.
DeSh output, such as responses to commands and other notifications can be echoed to the ``messages`` endpoint and the **Terminal** window of the nRF Cloud portal.
Use the ``print cloud enable`` command to enable this behavior.
The data format of the input data in the **Terminal** window must be JSON.

Examples
--------

* PT device: enabling printing also to cloud and establish the connection to nRF Cloud:

  .. code-block:: console

      desh:~$ print cloud enable

      desh:~$ cloud connect

* nRF Cloud: to request the DECT NR+ neighbor list for the PT device, enter the following command in the **Terminal** window of the nRF Cloud portal:

   .. code-block:: console

      {"appId":"DECT_SHELL", "data":"dect neighbor_list"}

  Response appears in the **d2c** terminal.

* nRF Cloud: to request icmpv6 ping towards Internet from PT device, enter the following command in the **Terminal** window of the nRF Cloud portal:

   .. code-block:: console

      {"appId":"DECT_SHELL", "data":"ping -d nordicsemi.com"}

  Response appears in the **d2c** terminal.

Iperf3
======

DeSh command: ``iperf``

Iperf3 is a tool for measuring data transfer performance both in uplink and downlink direction.

.. note::
   It is instructed to run the iperf3 server on the FT device and the iperf3 client on the PT device, i.e. direct connection between the devices.
   Some features, for example file operations and TCP option tuning, are not supported.

Examples
--------

* FT device: Create a network and see local IPv6 address for DECT NR+ networking interface:

  .. code-block:: console

      desh:~$ dect sett -t 1 --dev_type FT

      desh:~$ dect connect

      desh:~$ net iface 1
      Hostname: dect-ft-device.local
      Default interface: 1

      Interface nrf91_dect (0x20014758) (<unknown type>) [1]
      =========================================
      Interface is down.
      Link addr : 00:00:00:01:00:00:00:01
      MTU       : 1280
      Flags     : AUTO_START,IPv6,NO_ND
      Device    : nrf91_dect_mac_driver (0x4e9b4)
      Status    : oper=DORMANT, admin=UP, carrier=ON
      IPv6 unicast addresses (max 2):
            fe80::1:0:1 autoconf preferred infinite

* FT device: Start iperf3 server on the DECT NR+ networking interface on port 5555:

  .. code-block:: console

     desh:~$ iperf3 -s -B fe80::1:0:1 -p 5555 -1 -V -6

* PT device: Connect iperf3 client to the FT device iperf3 server on port 5555, using UDP protocol, with a payload size of 1220 bytes, for 50 seconds, and with a bandwidth of 2 Mbps:

  .. code-block:: console

     desh:~$ iperf3 -c fe80::1:0:1 -p 5555 -V -6 -u -l 1220 -t 50 -O 6 -b 2M

DK buttons
==========

The buttons have the following functions:

Button 1:
   Raises a kill or abort signal. A long press of the button will kill or abort all supported running commands. You can abort commands ``iperf3`` and ``ping``.

Building
********

.. |sample path| replace:: :file:`samples/dect/dect_mac/dect_shell`

.. include:: /includes/build_and_run_ns.txt

See Readme-build.rst for instructions on how to build different configurations.
Additionally, see :ref:`cmake_options` for instructions on how to provide CMake options, for example to use a configuration overlay.

Dependencies
************

It uses the following `sdk-nrfxlib`_ library:

* :ref:`nrfxlib:nrf_modem`
* :ref:`dk_buttons_and_leds_readme`

In addition, it uses the following secure firmware component:

* :ref:`Trusted Firmware-M <ug_tfm>`
