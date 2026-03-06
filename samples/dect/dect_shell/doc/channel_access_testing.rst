.. _dect_shell_channel_access_testing:

Channel Access Testing Guide
############################

This document describes how to use the DECT NR+ Shell sample (:file:`samples/dect/dect_shell`) as a testing tool for channel access conformance with `ETSI EN 301 406-2`_ (DECT-2020 NR Part 2: Physical layer).

.. note::
   This is **not a complete guide** to `ETSI EN 301 406-2`_ channel access testing. It covers the use of the DECT NR+ Shell for **random access** only. **Scheduled access** (ch. 4.5.4, 4.5.5 and test methods ch. 5.6.4, 5.6.5) is **not supported** by the sample or this guide.

.. contents::
   :local:
   :depth: 2

References
**********

* **`ETSI EN 301 406-2`_** (V3.1.1, 2023-08): Harmonised European Standard — DECT-2020 NR; Part 2: Physical layer (technical characteristics and methods of measurement).
* **`ETSI TS 103 636-4`_:** DECT-2020 New Radio (NR); Part 4: MAC layer (defines RSSI thresholds and procedures).
* **Sample:** :ref:`dect_shell_application` — nRF91x1 DECT NR+ Shell (networking stack, modem firmware v2.x).

ETSI EN 301 406-2: Channel Access Summary
*****************************************

The standard defines conformance requirements on channel access (ch. 4.5) and test methods (ch. 5.6). This guide and the DECT NR+ Shell focus on **random access** (FT channel selection and PT random access). **Scheduled access** (ch. 4.5.4, 4.5.5 and test methods ch. 5.6.4, 5.6.5) is not supported.

Requirements (ch. 4.5)
=========================

General (ch. 4.5.1)
---------------

* The EUT may support FT mode, PT mode, or both.
* The EUT **shall** support **random access** operation.
* Support for **scheduled** data transfer is optional (not supported by the DECT NR+ Shell; this guide covers random access only).

FT mode — Operating channel selection (ch. 4.5.2)
----------------------------------------------

* The EUT in FT mode **shall** scan operating radio frequency channels and measure **RSSI-1**.
* Channels are categorized per **Table 11**:

  * **Free:** max(RSSI-1) ≤ RSSI_THRESHOLD_MIN
  * **Possible:** RSSI_THRESHOLD_MIN < max(RSSI-1) ≤ RSSI_THRESHOLD_MAX
  * **Busy:** max(RSSI-1) > RSSI_THRESHOLD_MAX

  (Thresholds are defined in `ETSI TS 103 636-4`_.)
* The EUT **shall** select operating channel(s) from **Free** first; if none, from **Possible**; only if no other choice, from **Busy**.
* The EUT **shall** transmit in the selected operating channel(s).

PT mode — Random access (ch. 4.5.3)
-------------------------------

* The EUT in PT mode **shall** scan and select the companion device (FT) and perform random access as defined in `ETSI TS 103 636-4`_.
* The EUT **shall** transmit **only** on the operating channel instructed by the companion device in FT mode.
* The EUT **shall** monitor the companion device and:

  * Cease transmission on the operating channel **within 10 seconds** after receiving instruction (e.g. revoking resources, changing operating channel).
  * Cease transmission on the operating channel **within 30 seconds** after losing connection to the companion device.

Test methods (ch. 5.6)
=========================

* **ch. 5.6.1** — Generic procedure: spectrum analyser settings (Table 29), trace capture, threshold-based detection of TX on/off and silent periods.
* **ch. 5.6.2** — FT channel selection: Apply AWGN interference (ch. 5.3.2.3) over 40–80% of the operating band; verify with spectrum analyser that FT transmissions are on channels **not** (or less) interfered.
* **ch. 5.6.3** — PT random access: Setup FT + PT; verify PT follows RA resources; then revoke/change RA or remove companion; verify PT stops on current channel within 10 s (after instruction) or 30 s (after losing companion).
* **ch. 5.6.6** — Maximum transmission time in a 10 ms interval: requires temporal resolution ≤ 1 µs; not directly supported by the DECT NR+ Shell (see :ref:`dect_shell_channel_access_limitations`).

Using the DECT NR+ Shell for Channel Access Testing
****************************************************

The DECT NR+ Shell runs on top of the DECT NR+ **networking stack** (L2, Connection Manager) with modem firmware v2.x. Commands are implemented in the L2 shell library; the sample provides the application that uses them.

.. note::
   This sample is :file:`samples/dect/dect_shell`, not :file:`samples/dect/dect_phy/dect_shell`.
   The PHY shell offers additional tools (e.g. :command:`dect rf_tool`, LBT counters) not available here.

Prerequisites
=============

* At least **two** nRF91x1 development kits (one as EUT, one as companion) **plus an interferer**. The interferer can be:
  * An nRF91x1 DK running the **DECT PHY Shell** (:file:`samples/dect/dect_phy/dect_shell`), e.g. using :command:`dect rf_tool` or :command:`dect perf` to load channels; or
  * A signal generator (e.g. AWGN over 40–80% of the band per ch. 5.3.2.3).
* Ensure modem firmware v2.x and correct regional/band usage; see the sample README for regulations and channel frequencies.

Maximum transmitter activity (ch. 5.3.1 and ch. 5.6)
==================================================

The `ETSI EN 301 406-2`_ channel access test methods (e.g. ch. 5.6.2 Step 2, ch. 5.6.3 Step 2) require to *"Power on and configure the EUT to transmit with maximum transmitter activity supported by the EUT based on product information (see clause 5.3.1)."*

**What it means in the standard:**

* **Maximum transmitter activity** here means **maximum time on air** (duty cycle), not maximum TX power. Ch. 5.3.3.1 requires that the EUT use an operational mode that ensures *"a transmitter activity in a period of 10 ms to the maximum extent the device is supporting"* — i.e. the transmitter shall be active for as much of the 10 ms period as the device supports (within the limit of ch. 4.5.6, e.g. 23/24 × 10 ms). So the EUT is configured for **highest duty cycle / most TX usage**, not (by this phrase) for max output power. For other tests (e.g. RF output power, ch. 5.4.1) the standard separately requires the EUT to transmit at maximum RF output power.
* **Ch. 5.3.1** — Product information for testing: The manufacturer documents the operational mode(s) used for testing (e.g. power settings, modulation, worst case). That becomes the repeatable "product information" for the test report.

**In practice:** The EUT shall be in a mode where it is on air as much as possible during the test, so that channel access (and any cease-TX) behaviour is observable and representative of worst-case occupancy.

**With the DECT NR+ Shell:**

* **FT (EUT):** Create the network and start the cluster and network beacon so that the FT is transmitting regularly (cluster beacon at the chosen period, and optionally :command:`dect nw_beacon_start`). Use the shortest cluster beacon period and any other traffic the product supports to maximize FT transmitter activity (time on air). TX power is set separately (e.g. :command:`dect sett --cluster_max_beacon_tx_pwr`); for channel access tests, maximum activity is about duty cycle.
* **PT (EUT):** After associating, run traffic that keeps the PT using the random access resources as much as possible — e.g. :command:`iperf3` (high rate or long duration), repeated :command:`ping`, or periodic :command:`dect tx` / application data — so that the PT is transmitting at the maximum rate the stack allows during the test.

.. note::
   **Max duty cycle testing:** For controlled **maximum duty cycle** (e.g. `ETSI EN 301 406-2`_ ch. 4.5.6 / ch. 5.6.6 or representative TX patterns), use the :command:`dect rf_tool` in the **DECT PHY Shell** (:file:`samples/dect/dect_phy/dect_shell`). The DECT NR+ Shell does not provide rf_tool. **User IPv6 data:** In this sample, :command:`iperf3` and :command:`ping` (ICMPv6) can be used to generate user-plane IPv6 traffic over the DECT link, which exercises the stack and produces PT (or FT) transmissions for channel access observation.

RSSI scan and Free / Possible / Busy (ch. 4.5.2)
=================================================

**Command:** :command:`dect rssi_scan`

Execute RSSI measurement and obtain per-channel results (channel, all subslots free, busy percentage, free/busy subslot counts). The stack uses these to classify channels as Free, Possible, or Busy according to configured thresholds.

Options
-------

* ``-b <band>`` — Band number (e.g. ``1`` or ``2``). If given, channel list is ignored and all channels in the band are scanned.
* ``-c <channels>`` — Comma-separated channel list (max 20 channels).
* ``--frames <nbr_of_frames>`` — Number of frames to scan per channel.

Example
-------

.. code-block:: console

   desh:~$ dect rssi_scan -b 1 --frames 100

**Events:** ``NET_EVENT_DECT_RSSI_SCAN_RESULT`` (per channel), then ``NET_EVENT_DECT_RSSI_SCAN_DONE``.

Channel access thresholds (settings)
------------------------------------

Configure via :command:`dect sett`; read with :command:`dect sett -r`:

* ``--rssi_scan_time <msecs>`` — Scan time per channel (e.g. range 10–2550 ms).
* ``--rssi_scan_free_th <dbm>`` — **Free:** measured signal level ≤ value.
* ``--rssi_scan_busy_th <dbm>`` — **Busy:** measured signal level > value. **Possible:** between free and busy thresholds.
* ``--rssi_scan_suitable_percent <int>`` — SCAN_SUITABLE% as per spec.

.. _dect_shell_channel_access_settings_cease_tx:

Settings affecting channel access and cease-TX timing
-----------------------------------------------------

The following :command:`dect sett` options play an important role in channel access and in the timing of when the PT ceases transmission (`ETSI EN 301 406-2`_ ch. 5.6.3):

**FT side (cluster):**

* **``--cluster_nbr_inactivity_time <ms>``** — Neighbor inactivity timer (in ms) that triggers association release. Configured on the FT and used by the cluster: when a neighbor (PT) has been inactive for this duration, the FT can release the association. Tune this on the FT when preparing tests for the "within 10 seconds after instruction" or "within 30 seconds after losing connection" behaviour (ch. 4.5.3 / ch. 5.6.3).

**PT side (association / scan):**

* **``--max_beacon_rx_fails <int>``** — Maximum number of consecutive missed cluster beacons before the PT can release the association. Configured on the PT and passed to the modem when creating the association. Together with the cluster beacon period, it determines how quickly the PT detects loss of the companion and stops transmitting on the operating channel (relevant to the 30 s requirement).
* **``--min_sensitivity <dbm>``** — Minimum RX sensitivity (dBm) on the PT. Used when selecting an RD (FT) for association; beacons below this level are not considered suitable. Affects which channels/FTs the PT uses during scan and associate, and can influence channel access behaviour in dense or interfered environments.

Use :command:`dect sett -r` to inspect current values when preparing or analysing channel access tests.

.. code-block:: console

   desh:~$ dect sett -r

FT: Operating channel selection (ch. 5.6.2)
============================================

**Purpose:** Verify that a device in FT mode selects an operating channel from Free (or Possible/Busy per rules) and transmits on channels not interfered (`ETSI EN 301 406-2`_ ch. 5.6.2).

Procedure outline
-----------------

#. Apply wideband interference (40–80% of operating band) per ch. 5.3.2.3, or use the **interferer** to load a subset of channels (e.g. :command:`dect rf_tool` or :command:`dect perf` on the DECT PHY Shell).
#. On the EUT (FT), create a network so that the stack performs RSSI scan and selects a free channel.
#. Using a spectrum analyser and the procedure in ch. 5.6.1, verify that FT transmissions are on channels **not** (or less) interfered.

Commands
--------

**Automatic channel selection (recommended):**

.. code-block:: console

   desh:~$ dect sett --dev_type FT -t 1
   desh:~$ dect connect
   # Wait for "Cluster started/reconfigured at channel <N>"
   # Wait for "Network status: created"

   desh:~$ dect cluster_info
   # See cluster information

**Alternative: Manual cluster start with automatic channel selection:**

.. code-block:: console

   desh:~$ dect sett --dev_type FT -t 1
   desh:~$ dect cluster_start
   # Wait for "Cluster started/reconfigured at channel <N>"

* **Band:** :command:`dect sett -b 2` (or ``-b 1`` (the default)) sets the band used for network creation and channel selection.
* **Channel reselection:** Settings such as ``--cluster_ch_reselection_th`` and ``--cluster_channel_loaded_percent`` control when the FT automatically reselects channel (e.g. when the **interferer** causes interference). Use :command:`dect sett -r` to inspect.

PT: Random access and ceasing transmission (ch. 5.6.3)
========================================================

**Purpose:** Verify that a device in PT mode follows the companion FT's random-access configuration and stops transmitting on the operating channel after instruction (revoke/change) or after losing the companion device (`ETSI EN 301 406-2`_ ch. 5.6.3).

Step 1–2 — Setup and verify PT on RA resources
-----------------------------------------------

#. On companion (FT): create network and start beacon, e.g.:

   .. code-block:: console

      desh:~$ dect sett --dev_type FT -t 1
      desh:~$ dect connect
      # Wait for "Cluster started/reconfigured at channel <N>" (FT)
      # Wait for "Network status: created" (FT)

#. On EUT (PT): scan and associate:

   .. code-block:: console

      desh:~$ dect sett --dev_type PT -t 2 --nw_join_target 1
      desh:~$ dect connect
      # Wait for "Network status: joined"

#. Verify PT is using the channel/RA indicated by the FT (also e.g. from traffic or spectrum analyser).

   .. code-block:: console

      desh:~$ dect status
      # Verify "Associations:" shows "Parent long RD ID: 1 (0x00000001)"

      desh:~$ dect neighbor_list
      # Verify "Neighbors:" shows "Parent long RD ID: 1 (0x00000001)"

      desh:~$ dect neighbor_info 1
      # Verify "Channel: <N>" matches the FT's cluster channel.

Step 3–4 — Revoke or change RA / operating channel
---------------------------------------------------

* On companion FT (preferred option): manually change cluster channel:

  .. code-block:: console

     desh:~$ dect cluster_reconfig -c <new_channel>
     # Wait for "Cluster started/reconfigured at channel <new_channel>"

* Alternatively: use the **interferer** (e.g. :command:`dect perf` on the DECT PHY Shell) to load the cluster channel with high load that exceeds the threshold for triggering automatic reselection (``--cluster_channel_loaded_percent``) for the FT device cluster.
Verify in FT device that channel reselection occurs.

  .. code-block:: console

     # Wait for "Cluster channel load (90%) exceeded threshold (80%): channel <current_channel>"
     # Wait for "Cluster started/reconfigured at channel <new_channel>"

     desh:~$ dect cluster_info
     # Verify "Channel: <new_channel>"

#. Verify PT is using the channel/RA indicated by the FT.

   .. code-block:: console

      desh:~$ dect status
      # Verify "Associations:" shows "Parent long RD ID: 1 (0x00000001)"

      desh:~$ dect neighbor_list
      # Verify "Neighbors:" shows "Parent long RD ID: 1 (0x00000001)"

      desh:~$ dect neighbor_info 1
      # Verify "Channel: <N>" matches the FT's new cluster channel.

* Using spectrum analyser (and procedure in ch. 5.6.1), verify that the PT **stops** transmitting on the **current** operating channel within **10 seconds** after the change/revoke (reference: end of next transmission occurrence after step 3).

Step 6–7 — Remove companion transmission
-----------------------------------------

* On FT: stop network beacon and/or remove network:

  .. code-block:: console

     desh:~$ dect disconnect
     # Wait for "Network status: removed" (FT)

* On PT: observe the network status:

  .. code-block:: console

     # Verify "Association released with long RD ID: 1 (0x00000001)"
     # Verify "Network status: unjoined" (PT)
     desh:~$ dect status
     # Verify that no "Associations:" are shown

* Verify with spectrum analyser that the PT **stops** transmitting on the operating channel within **30 seconds** (reference: after step 6, end of 10th transmission occurrence by PT).

  .. note::
     The timing of association release and cease-TX depends on settings: on the **FT**, ``--cluster_nbr_inactivity_time``; on the **PT**, ``--max_beacon_rx_fails`` and the cluster beacon period. See :ref:`Settings affecting channel access and cease-TX timing <dect_shell_channel_access_settings_cease_tx>` when tuning for the 10 s / 30 s requirements.

**PT release association:**

.. code-block:: console

   desh:~$ dect dissociate -t 1
   # Or:
   desh:~$ dect nw_unjoin

Region and channel access
=========================

.. code-block:: console

   desh:~$ dect sett --region eu
   # Or: us, global

The region can impact channel access behaviour; set as required by your test plan.

Command reference (channel access related)
******************************************

.. list-table:: Channel access related commands
   :widths: 30 50
   :header-rows: 1

   * - Command
     - Description
   * - :command:`dect rssi_scan`
     - Execute RSSI scan; options: ``-b``, ``-c``, ``--frames``.
   * - :command:`dect sett`
     - Read/write settings (rssi_scan_*, cluster_*,region). Use ``-r`` to read, ``--reset`` to reset.
   * - :command:`dect cluster_start [channel]`
     - Start cluster; optional channel or omit for auto selection.
   * - :command:`dect cluster_reconfig [-c <channel>]`
     - Reconfigure cluster; ``-c <channel>`` for new channel.
   * - :command:`dect nw_create`
     - Create network (RSSI scan + cluster start + optional NW beacon).
   * - :command:`dect nw_beacon_start -c <ch> [--add_channels]`
     - Start network beacon on channel; optional ``--add_channels``.
   * - :command:`dect nw_beacon_stop`
     - Stop network beacon.
   * - :command:`dect nw_remove`
     - Remove network (FT).
   * - :command:`dect scan`
     - PT: scan for cluster/NW beacons; ``-b <band>``.
   * - :command:`dect associate -t <id>`
     - PT: associate with FT by long RD ID.
   * - :command:`dect dissociate -t <id>`
     - PT: release association.
   * - :command:`dect nw_join`
     - PT: scan + associate (join network).
   * - :command:`dect nw_unjoin`
     - PT: leave network.
   * - :command:`dect connect`
     - FT: create network; PT: create cluster/network.
   * - :command:`dect disconnect`
     - FT: remove network; PT: leave network.
   * - :command:`dect status`
     - Show modem, cluster, beacon, associations.

.. _dect_shell_channel_access_limitations:

Limitations
***********

The DECT NR+ Shell (:file:`samples/dect/dect_shell`) does **not** provide:

* **PHY-level rf_tool** — No :command:`dect rf_tool` for RX/TX patterns, LBT failure counts, or duty-cycle reporting. Use the **DECT PHY Shell** (:file:`samples/dect/dect_phy/dect_shell`) and its :command:`dect rf_tool` for **transmitter and receiver conformance tests** (`ETSI EN 301 406-2`_ ch. 4.3, 4.4 and ch. 5.4, 5.5) and for **maximum duty cycle testing** (e.g. ch. 4.5.6 / ch. 5.6.6). For maximum transmission time verification, use rf_tool with external measurement (e.g. power sensor with ≤ 1 µs resolution).
* **PHY ping/perf** — No :command:`dect ping` or :command:`dect perf`. In this sample, :command:`ping` (ICMPv6) and :command:`iperf3` can be used to produce **user IPv6 data** over the DECT link for connectivity, throughput, and channel access observation.
* **Explicit LBT CLI** — LBT is in the modem/MAC; channel reselection (e.g. when the **interferer** causes interference) is observed via cluster reconfigure or automatic reselection, not via "LBT BUSY" counters in the shell.
* **Verdict_type_count / verdict_type_count_details** — RSSI results are "All subslots free", "Busy percentage", and subslot counts; the PHY shell offers additional verdict options.

For PHY-level and LBT-focused channel access tests, use the **DECT PHY Shell** sample (:file:`samples/dect/dect_phy/dect_shell`).

.. note::
   For **transmitter and receiver conformance tests** (`ETSI EN 301 406-2`_ ch. 4.3 and 4.4, e.g. RF output power, centre frequencies, receiver sensitivity, adjacent channel selectivity), use the :command:`dect rf_tool` and related PHY commands from the **DECT PHY Shell** sample (:file:`samples/dect/dect_phy/dect_shell`). That sample is intended for running `ETSI EN 301 406-2`_ transmitter and receiver test methods (ch. 5.4 and 5.5).

ETSI clause to DeSh usage mapping
*********************************

.. list-table:: ETSI clause to DeSh usage
   :widths: 25 55
   :header-rows: 1

   * - ETSI clause
     - Use of :file:`samples/dect/dect_shell`
   * - ch. 4.5.2 / ch. 5.6.2 (FT channel selection)
     - :command:`dect rssi_scan` + sett thresholds; :command:`dect connect` or :command:`dect nw_create` or :command:`dect cluster_start` (auto channel). Verify with spectrum analyser.
   * - ch. 4.5.3 / ch. 5.6.3 (PT RA, cease TX)
     - :command:`dect scan` → :command:`dect associate` (or :command:`dect nw_join`). Revoke/change: :command:`dect cluster_reconfig -c <ch>` or stop network. Remove companion: :command:`dect nw_beacon_stop` / :command:`dect nw_remove`. Verify PT stops on channel (spectrum analyser).
   * - ch. 4.5.6 / ch. 5.6.6 (Max TX time)
     - Not supported; use app traffic + external timing measurement.
   * - FT channel reselection
     - Settings ``cluster_ch_reselection_th`` / ``channel_loaded_percent``; use the **interferer** to create interference; observe cluster move or :command:`dect cluster_reconfig`.
