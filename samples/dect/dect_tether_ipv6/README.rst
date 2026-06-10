.. _dect_tether_ipv6_sample:

nRF91x1: DECT IPv6 tethering (RA + DHCPv6 + mDNS forward)
#########################################################

.. contents::
   :local:
   :depth: 2

This sample is built as a **PT** (Portable Termination) device by default; merge :file:`pt.conf` so ``CONFIG_DECT_DEFAULT_DEV_TYPE_PT`` is explicit (see `Building`_). Use :file:`ft.conf` only when you need **FT** instead.

The sample enables ``CONFIG_DECT_TETHER_IPV6_LIB`` for a tethered host on **Ethernet**:

* **ICMPv6 Router Advertisements** on ``eth0``: **default router** (``router_lifetime``), **RDNSS** (DNS), **Managed (M)** flag set (``CONFIG_DECT_TETHER_IPV6_RA_MANAGED=y``, required), and **Prefix Information Options** for ULA/GUA with **L=0** and **A=0** (prefix hint only — no SLAAC, not on-link).
* **Minimal DHCPv6 server** on UDP port **547**: responds to **Solicit**, **Request**, **Renew**, and **Information-request**, offering **ULA** and **GUA** ``/128`` values read from ``dect0`` (same addresses the stack configured on the DECT interface).

The tether model is **split**: RA advertises this device as the IPv6 default gateway; **M=1** tells the host to obtain those DECT **/128** addresses via DHCPv6 **IA_NA**. Do **not** clear the Managed flag (``CONFIG_DECT_TETHER_IPV6_RA_MANAGED=n``): with PIO **A=0** there is no SLAAC fallback, so hosts such as Windows typically end up with **no addresses and no** ``::/0``.

On the nRF9151 DK, console and shell use **UART0** at 115200 baud (board default), like other Zephyr samples on that kit.

Requirements
************

The sample supports the following development kit:

.. table-from-sample-yaml::

For **Ethernet** builds, use an SPI Ethernet shield so ``eth0`` exists; merge the overlays described in :ref:`dect_tether_ipv6_building`.

.. include:: /includes/tfm.txt

Overview
********

Call ``dect_tether_ipv6_init()`` after ``nrf_modem_lib_init()`` (see :file:`src/main.c`).

The following abbreviations from the DECT NR+ MAC specification (`ETSI TS 103 636-4`_) are used where relevant:

* FT: Fixed Termination point
* PT: Portable Termination point

Tethered hosts (Windows and Ubuntu)
===================================

The same tether-gateway firmware behaves differently on **Windows 11** and **Ubuntu** when using baseline overlays (``eth_common.conf`` only).

**Ubuntu** typically keeps an RA-learned ``::/0`` (``ip -6 route show default``) and refreshes the route **expires** timer when multicast RAs arrive (~every **60 s**). DHCPv6 on the PC often completes in one round trip (**Solicit → Reply** when the client uses Rapid Commit).

**Windows 11** may **remove** the RA-learned default route after NUD (~30 s with default ``REACHABLE_TIME``) even though the tether gateway still sends multicast RAs and DHCPv6 addresses may still work. The PowerShell monitor below prints **GW GONE** when ``::/0`` is missing. For long sessions on Windows, add a persistent static ``::/0`` on the PC (see *Optional static default gateway* below) or append :file:`eth_max_connectivity.conf`.

**Default route vs neighbor cache** — these are independent on the host:

* **``::/0`` / GW valid** — from the RA **router_lifetime** (and Windows route policy).
* **Neighbor ``fe80::…``** — link-layer mapping; may show **Reachable**, **Stale**, or **Permanent** (if you pinned it with ``netsh``). A **Reachable** neighbor does not guarantee Windows will keep ``::/0``.

RA timing (firmware defaults)
-----------------------------

Unless an overlay changes them, Kconfig defaults in ``nrf/subsys/net/lib/dect/dect_tether_ipv6/Kconfig`` are:

.. list-table::
   :header-rows: 1

   * - Option
     - Default
     - Role
   * - ``RA_ROUTER_LIFETIME``
     - **1800 s** (30 min)
     - How long the host should keep this device as the IPv6 default router
   * - ``RA_REACHABLE_TIME_MS``
     - **30000 ms** (30 s)
     - NUD hint: hosts often probe the ``fe80::`` next hop after ~30 s without traffic
   * - ``RA_RS_UNICAST``
     - **y** with :file:`eth_common.conf`
     - Unicast RA to each RS source (in addition to multicast)
   * - ``RA_UNSOLICIT_MS``
     - **60000 ms** (60 s)
     - Periodic **multicast** RA on ``eth0``
   * - ``RA_PIO_VALID_LIFETIME`` / ``RA_PIO_PREFERRED_LIFETIME``
     - **86400 s** / **7200 s**
     - Prefix Information Option lifetimes (PIO is **L=0**, **A=0**; hint only)

The tether gateway also sends RAs on **Ethernet link up**, after **link-local DAD**, and on each **Router Solicitation** (multicast RA to ``ff02::1``, plus **unicast** RA to the RS source when :file:`eth_common.conf` is used — ``RA_RS_UNICAST``).

:file:`eth_ra_test_short_lifetime.conf` sets ``RA_ROUTER_LIFETIME=120`` and **30 s** periodic RAs for lab scripts only — **not** the product default. If the Windows monitor shows ``RL=00:02:00`` counting down, check that this overlay is not in ``EXTRA_CONF_FILE``.

Confirm built values:

.. code-block:: console

   grep CONFIG_DECT_TETHER_IPV6_RA_ build/*/zephyr/.config

Windows 11
------------

RA **Managed (M)** and DHCPv6
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Keep ``CONFIG_DECT_TETHER_IPV6_RA_MANAGED=y`` (Kconfig default) for this sample. It is required for the intended tether-gateway behaviour:

* **DHCPv6** — leases the DECT **ULA** and **GUA** ``/128`` addresses to the PC.
* **RA** — installs the **default route** (``::/0``) via ``router_lifetime`` while M directs address configuration to DHCPv6.

After experiments that disabled M, refresh the PC stack (adapter disable/enable or ``ipconfig /release6``) before retesting.

Use an **elevated** PowerShell or Command Prompt when releasing DHCPv6 or restarting adapters.

* **Refresh DHCPv6 leases** after reflashing the tether gateway or changing its Ethernet MAC:

  .. code-block:: bat

     ipconfig /release6
     ipconfig /renew6

* **Restart the Ethernet adapter** if the link or neighbor cache looks stale (use the exact alias from ``Get-NetAdapter``):

  .. code-block:: powershell

     Restart-NetAdapter -Name "Ethernet" -Confirm:$false

* **List IPv6 addresses on the tether NIC**:

  .. code-block:: powershell

     Get-NetIPAddress -InterfaceAlias "Ethernet" -AddressFamily IPv6

  or:

  .. code-block:: bat

     netsh interface ipv6 show address interface="Ethernet"

* **Ping the tether gateway link-local** — append the interface zone index from ``ipconfig`` (the ``%`` number after ``fe80::…``):

  .. code-block:: bat

     ping -6 fe80::<tether-gateway-link-local>%<zone>

Firmware overlays vs PC-side workarounds
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Optional overlays** — append to ``EXTRA_CONF_FILE`` after :file:`eth_common.conf`:

* :file:`eth_max_connectivity.conf` — long ``REACHABLE_TIME``, long router/DHCP lifetimes (NUD about every 30–90 min on Windows instead of ~30 s). Use for long **iperf** runs or when the monitor shows **GW GONE** after the second NUD on baseline firmware.
* :file:`eth_ra_test_short_lifetime.conf` — short ``router_lifetime`` and **30 s** periodic RAs for lab scripts.

**Baseline firmware** (no overlay) sends **multicast-only** periodic RAs. Windows 11 often **drops** the RA-learned default router while addresses from DHCPv6 still work. That shows up as **GW GONE** in the monitor below even when DNS still works.

**Optional PC workaround** (below) — persistent ``::/0`` and neighbor to the tether gateway link-local. Use when testing baseline firmware, when the monitor shows **GONE**, or when a bogus ``::/0`` to another ``fe80::`` appears (see *Remove a wrong gateway*).

Find the tether NIC and tether-gateway link-local
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Run in **elevated** PowerShell. Replace ``"Ethernet"`` with the alias from ``Get-NetAdapter`` if your tether NIC has another name.

**1. Tether adapter and interface index**

.. code-block:: powershell

   Get-NetAdapter | Format-Table Name, ifIndex, Status, LinkSpeed, InterfaceDescription

**2. PC GUA from DHCPv6 (confirms which ifIndex is the tether)**

.. code-block:: powershell

   Get-NetIPAddress -AddressFamily IPv6 -PrefixOrigin Dhcp |
     Format-Table InterfaceAlias, ifIndex, IPAddress

**3. Tether-gateway link-local** — from a Wireshark RA **Source** on Ethernet (SLLAO MAC ``f6:ce:36:04:73:b1`` → ``fe80::f4ce:36ff:fe04:73b1`` with a fixed W5500 MAC), or from the device log / ``net iface`` on the nRF. Example:

.. code-block:: text

   fe80::f4ce:36ff:fe04:73b1

**4. Ping the tether gateway** — use the tether **ifIndex** from step 1 (not a guessed zone id):

.. code-block:: powershell

   ping -6 fe80::f4ce:36ff:fe04:73b1%29

Check status (routes and neighbors)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**All default routes** (every interface):

.. code-block:: powershell

   Get-NetRoute -AddressFamily IPv6 -DestinationPrefix "::/0" |
     Format-Table ifIndex, InterfaceAlias, NextHop, RouteMetric, ifMetric, ValidLifetime, PolicyStore

Prefer **one** ``::/0`` on the tether with **lowest** ``RouteMetric`` (and lower ``ifMetric`` than other NICs). If two ``::/0`` exist with the same metric, Windows prefers the interface with the **lower ifMetric**.

**Default route on the tether only**

.. code-block:: powershell

   Get-NetRoute -InterfaceAlias "Ethernet" -AddressFamily IPv6 -DestinationPrefix "::/0" |
     Format-Table ifIndex, NextHop, RouteMetric, ValidLifetime, PolicyStore

**Neighbors on the tether only** (gateway should be ``Reachable`` or ``Permanent``; MAC ``F6-CE-36-04-73-B1`` for the example above):

.. code-block:: powershell

   Get-NetNeighbor -InterfaceAlias "Ethernet" -AddressFamily IPv6 |
     Format-Table IPAddress, LinkLayerAddress, State, PolicyStore

**Link-local neighbors only**

.. code-block:: powershell

   Get-NetNeighbor -InterfaceAlias "Ethernet" -AddressFamily IPv6 |
     Where-Object { $_.IPAddress -like "fe80*" } |
     Format-Table IPAddress, LinkLayerAddress, State

**All IPv6 neighbors (every interface)**

.. code-block:: powershell

   Get-NetNeighbor -AddressFamily IPv6 |
     Format-Table InterfaceAlias, IPAddress, LinkLayerAddress, State

**IPv6 addresses on the tether**

.. code-block:: powershell

   Get-NetIPAddress -InterfaceAlias "Ethernet" -AddressFamily IPv6 |
     Format-Table IPAddress, PrefixOrigin, SuffixOrigin

or:

.. code-block:: bat

   netsh interface ipv6 show addresses

``netsh interface ipv6 show route`` lists all routes (no per-interface filter). ``netsh interface ipv6 show routers`` is **not** available on many Windows 11 builds.

Live **GW valid / GONE** monitor (tether ``::/0``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Paste into **elevated** PowerShell. Watches ``Get-NetRoute`` on **Ethernet** (change the alias if needed). Stop with **Ctrl+C**.

* **``RL=static``** (very large ``ValidLifetime``) — persistent route you added with ``store=persistent``.
* **``RL=00:30:xx``** counting down — typical with default ``RA_ROUTER_LIFETIME=1800`` s.
* **``RL=00:02:00``** — usually :file:`eth_ra_test_short_lifetime.conf` (``router_lifetime=120``), not the Kconfig default.
* **``RL`` jumps back to ~2:00 or ~30:00** — a new RA refreshed ``router_lifetime`` (good).
* **``GW GONE``** — no ``::/0`` on that interface (common on Windows after NUD; add static route below, or :file:`eth_max_connectivity.conf`).

.. code-block:: powershell

   while ($true) {
       $r = Get-NetRoute -InterfaceAlias "Ethernet" -DestinationPrefix "::/0" -ErrorAction SilentlyContinue
       if ($r) {
           $rl = $r.ValidLifetime
           if ($rl.TotalDays -gt 3650) { $rlStr = "static" } else { $rlStr = $rl.ToString() }
           Write-Host "$(Get-Date -Format HH:mm:ss)  GW valid  RL=$rlStr  NextHop=$($r.NextHop)  Metric=$($r.RouteMetric)" -ForegroundColor Green
       } else {
           Write-Host "$(Get-Date -Format HH:mm:ss)  GW GONE" -ForegroundColor Red
       }
       Start-Sleep 1
   }

Optional static default gateway (Windows 11)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use when the monitor shows **GONE**, ping to the tether gateway fails, or multicast RAs do not refresh ``::/0``. A persistent static route keeps the tether path without changing firmware. Run commands in an **elevated** PowerShell or Command Prompt.

Replace ``Ethernet`` with your tether adapter alias (``Get-NetAdapter``), and replace ``fe80::f4ce:36ff:fe04:73b1`` / ``f6-ce-36-04-73-b1`` with the tether-gateway link-local and Ethernet MAC from the device log or Wireshark.

**Side effect:** a persistent ``::/0`` on the tether NIC tells Windows to send **most IPv6** via the tether gateway (next hop = tether-gateway link-local). That is intentional if you need reachability on ``eth0``, but it can make it feel like “all traffic” goes through the nRF path. **IPv4 is unchanged** unless you also changed IPv4 routes. Remove the static route when you are done (*Remove static tether default route* below).

**Add persistent default route** — use metric **256** (same as typical RA) if the PC must stay on another network for internet; use metric **1** only when the tether must override other NICs:

.. code-block:: powershell

   netsh interface ipv6 delete route ::/0 "Ethernet" fe80::f4ce:36ff:fe04:73b1 store=persistent
   netsh interface ipv6 add route ::/0 "Ethernet" fe80::f4ce:36ff:fe04:73b1 store=persistent metric=256

If the tether must win over Wi‑Fi/other ``::/0`` (lab only):

.. code-block:: powershell

   netsh interface ipv6 set route ::/0 "Ethernet" fe80::f4ce:36ff:fe04:73b1 metric=1 store=persistent

If ``The object already exists``, delete first (as above) or change the metric with ``set route`` as above.

**Pin tether-gateway neighbor** (if ping fails with ``General failure`` or neighbor stays ``Stale``):

.. code-block:: powershell

   netsh interface ipv6 add neighbors "Ethernet" fe80::f4ce:36ff:fe04:73b1 f6-ce-36-04-73-b1 store=persistent

**Verify**

.. code-block:: powershell

   Get-NetRoute -AddressFamily IPv6 -DestinationPrefix "::/0" | Format-Table ifIndex, InterfaceAlias, NextHop, RouteMetric, ifMetric
   Get-NetNeighbor -InterfaceAlias "Ethernet" -AddressFamily IPv6 | Where-Object { $_.IPAddress -like "fe80*" }
   ping -6 fe80::f4ce:36ff:fe04:73b1%29
   ping -6 8.8.8.8

Remove a wrong gateway or neighbor
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Windows may install a bogus ``::/0`` via another ``fe80::`` (often **metric 1**), e.g. ``fe80::725c:f964:66c0:5dfd``, while the real tether gateway is ``fe80::f4ce:36ff:fe04:73b1``. Delete the wrong route and neighbor; keep only the tether gateway.

.. code-block:: powershell

   Get-NetRoute -AddressFamily IPv6 -DestinationPrefix "::/0" | Format-Table ifIndex, NextHop, RouteMetric, ifMetric

   netsh interface ipv6 delete route ::/0 "Ethernet" fe80::725c:f964:66c0:5dfd store=persistent
   netsh interface ipv6 delete route ::/0 "Ethernet" fe80::725c:f964:66c0:5dfd store=active

   netsh interface ipv6 delete neighbors "Ethernet" fe80::725c:f964:66c0:5dfd store=persistent
   netsh interface ipv6 delete neighbors "Ethernet" fe80::725c:f964:66c0:5dfd store=active

Then re-add the correct tether-gateway route (see *Optional static default gateway* above).

Remove static tether default route
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When testing is done, or traffic should use Wi‑Fi/main LAN again:

.. code-block:: powershell

   netsh interface ipv6 delete route ::/0 "Ethernet" fe80::f4ce:36ff:fe04:73b1 store=persistent
   netsh interface ipv6 delete route ::/0 "Ethernet" fe80::f4ce:36ff:fe04:73b1 store=active
   netsh interface ipv6 delete neighbors "Ethernet" fe80::f4ce:36ff:fe04:73b1 store=persistent
   netsh interface ipv6 delete neighbors "Ethernet" fe80::f4ce:36ff:fe04:73b1 store=active

Then refresh DHCPv6 (``ipconfig /release6`` / ``renew6``) or disable/enable the adapter so Windows reinstalls the RA default route if the tether gateway sends RAs.

Other useful commands (Windows)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* **Refresh DHCPv6** after reflashing the tether gateway or changing its Ethernet MAC:

  .. code-block:: bat

     ipconfig /release6
     ipconfig /renew6

* **Restart the tether adapter**:

  .. code-block:: powershell

     Restart-NetAdapter -Name "Ethernet" -Confirm:$false

* **Trigger DHCPv6 Renew on one interface**:

  .. code-block:: bat

     ipconfig /renew6 "Ethernet"

* **Watch routes, neighbors, and addresses** (refresh every 2 s):

  .. code-block:: powershell

     while ($true) {
         Clear-Host
         Write-Host "=== ::/0 routes ==="
         Get-NetRoute -AddressFamily IPv6 -DestinationPrefix "::/0" |
           Format-Table InterfaceAlias, NextHop, RouteMetric, ifMetric, PolicyStore
         Write-Host "=== Ethernet ::/0 ==="
         Get-NetRoute -InterfaceAlias "Ethernet" -DestinationPrefix "::/0" -ErrorAction SilentlyContinue |
           Format-Table NextHop, RouteMetric, ValidLifetime
         Write-Host "=== Ethernet neighbors (fe80) ==="
         Get-NetNeighbor -InterfaceAlias "Ethernet" -AddressFamily IPv6 |
           Where-Object { $_.IPAddress -like "fe80*" } |
           Format-Table IPAddress, LinkLayerAddress, State
         Write-Host "=== Addresses ==="
         Get-NetIPAddress -InterfaceAlias "Ethernet" -AddressFamily IPv6 |
           Format-Table IPAddress, PrefixOrigin
         Start-Sleep 2
     }

Ubuntu
------

Baseline firmware is usually enough on Ubuntu.

Check default gateway and routes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   ip -6 route show default
   ip -6 route show dev eth0
   ip -6 addr show dev eth0
   ip -6 neigh show dev eth0

Example default route (``proto ra`` = Router Advertisement):

.. code-block:: text

   default via fe80::f4ce:36ff:fe04:73b1 dev eth0 proto ra metric 1024 expires 1747sec

Refresh DHCPv6 after reflashing the tether gateway:

.. code-block:: bash

   sudo dhclient -6 -r eth0
   sudo dhclient -6 eth0

Live **GW valid / GONE** monitor (bash)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Same idea as the Windows PowerShell loop: one line per second. Replace ``eth0`` with your tether interface. Stop with **Ctrl+C**.

.. code-block:: bash

   IF=eth0
   while true; do
     line=$(ip -6 route show default dev "$IF" 2>/dev/null | head -1)
     if [ -n "$line" ]; then
       via=$(echo "$line" | sed -n 's/.* via \([^ ]*\).*/\1/p')
       metric=$(echo "$line" | sed -n 's/.* metric \([0-9]*\).*/\1/p')
       expires=$(echo "$line" | sed -n 's/.* expires \([^ ]*\).*/\1/p')
       proto=$(echo "$line" | sed -n 's/.* proto \([^ ]*\).*/\1/p')
       printf '%s  GW valid  expires=%s  via=%s  metric=%s  proto=%s\n' \
         "$(date +%H:%M:%S)" "${expires:-n/a}" "$via" "${metric:-?}" "${proto:-?}"
     else
       printf '%s  GW GONE\n' "$(date +%H:%M:%S)"
     fi
     sleep 1
   done

* **``expires=…sec``** — kernel countdown until the RA route expires; should **jump back** when a new RA arrives (~60 s on baseline).
* **``GW GONE``** — rare on Ubuntu with baseline firmware; investigate if the tether gateway stopped sending RAs or the link is down.

Optional: color **GONE** in red:

.. code-block:: bash

   IF=eth0
   while true; do
     if ip -6 route show default dev "$IF" 2>/dev/null | grep -q .; then
       echo "$(date +%H:%M:%S)  GW valid  $(ip -6 route show default dev "$IF" | head -1)"
     else
       echo -e "\033[31m$(date +%H:%M:%S)  GW GONE\033[0m"
     fi
     sleep 1
   done

Capture RAs on the tether (optional):

.. code-block:: bash

   sudo tcpdump -ni eth0 'icmp6 and ip6[40] == 134'

.. _dect_tether_ipv6_building:

Building
********

.. |sample path| replace:: :file:`samples/dect/dect_tether_ipv6`

.. include:: /includes/build_and_run_ns.txt

See :ref:`cmake_options` for instructions on how to provide CMake options, for example to use a configuration overlay.

Configuration overlays (Ethernet)
=================================

Baseline **Kconfig** (no extra overlay) provides standard tethering: **Managed** RA, DHCPv6 **/128** leases, **60 s** multicast RAs, RS-triggered RAs, and Zephyr **solicited NA** to host NUD (see *RA timing (firmware defaults)* above). **Ubuntu** tether tests usually need only baseline.

Optional files in this sample directory (append in this order after :file:`eth_common.conf` and shield-specific :file:`eth_*.conf`):

:file:`mdns-common.conf`
  Zephyr mDNS / DNS-SD stack (optional).

:file:`mdns.conf`
  Sample DNS-SD advertisement on ``dect0``.

:file:`eth_mdns.conf`
  mDNS-on-Ethernet tuning (scoped DNS, ``CONFIG_ZVFS_POLL_MAX``). After :file:`mdns-common.conf` and :file:`mdns.conf`.

:file:`mdns_forward.conf`
  eth0 ↔ dect0 mDNS tap. After :file:`eth_mdns.conf`.

:file:`eth_max_connectivity.conf`
  RFC-max **REACHABLE_TIME** (1 h), long **router_lifetime**, long DHCPv6 lifetimes. For long **iperf** / stability tests on Windows.

:file:`eth_ra_test_short_lifetime.conf`
  Lab helper: ``router_lifetime=120`` and **30 s** periodic RAs.

:file:`eth_unsol_na.conf`
  Periodic unsolicited **Neighbor Advertisements** on ``eth0`` (optional ND-table aid).

:file:`dect_rx_pool.conf`
  Enables a **DECT-private RX net_pkt / net_buf pool** (see :ref:`dect_tether_ipv6_dect_rx_pool`) so eth0 RX bursts cannot starve dect0 RX.

Example — Ethernet tether + W5500 (from the sample directory):

.. code-block:: console

   cd nrf/samples/dect/dect_tether_ipv6
   west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_w5500.conf" -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

**PT (default)** — merge :file:`pt.conf` (Zephyr DECT Kconfig also defaults to PT if you omit it, but the sample documents :file:`pt.conf` for clarity):

.. code-block:: console

   cd nrf/samples/dect/dect_tether_ipv6
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE=pt.conf

**FT** — when the kit should act as Fixed Termination (for example Ethernet sink to a PC), use :file:`ft.conf` instead of :file:`pt.conf`:

.. code-block:: console

   cd nrf/samples/dect/dect_tether_ipv6
   west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE=ft.conf

Ethernet with W5500 shield (Arceli)
===================================

Use this when the host leg should use **Ethernet** via the Zephyr :ref:`arceli_eth_w5500` shield on the nRF9151 DK.
Merge :file:`pt.conf`, :file:`eth_common.conf`, and :file:`eth_w5500.conf` (PT default); devicetree comes from :file:`w5500-static-mac.overlay` in this sample plus the Zephyr shield devicetree :file:`zephyr/boards/shields/arceli_eth_w5500/arceli_eth_w5500.overlay`.

.. note::
   Arduino **D8** (reset) and **D9** (interrupt) are shared with **BUTTON1** and **BUTTON2** on the nRF9151 DK.
   The Ethernet Kconfig overlays disable the DK library to avoid conflicts with the Arceli shield.

* From the sample directory (copy-paste each line):

  .. code-block:: console

     cd nrf/samples/dect/dect_tether_ipv6
     west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_w5500.conf" -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

* Wiring as in the Arceli ETH W5500 shield overlay. Connect the RJ45 port to your LAN (router/switch) or directly to a PC as needed for your test.

**Ethernet MAC:** :file:`w5500-static-mac.overlay` sets a fixed locally administered ``local-mac-address`` — edit that file so each board on the same LAN is unique.
For a random MAC each boot (``zephyr,random-mac-address``), use ``-DDTC_OVERLAY_FILE=w5500.overlay`` instead.

.. table:: nRF9151 DK + Arceli ETH W5500 (Arduino header).

   +-----------------------------+------------------------------------------+
   | W5500 / shield signal       | nRF9151 DK (Arduino / GPIO)              |
   +=============================+==========================================+
   | **SCS**                      | **D10** (**P0.10**)                      |
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
====================================================

WARNING: Wiznet w5500 shield (red one) is not working correctly and can burn your DK!

Use this when the host leg should use **Ethernet** via the Zephyr :ref:`seeed_w5500` shield on the nRF9151 DK.
Merge :file:`pt.conf`, :file:`eth_common.conf`, :file:`eth_w5500.conf`, and :file:`eth_w5500_seeed.conf` (PT default).
The Seeed shield (Rev 1.01) leaves the W5500 INTn disconnected, so :file:`eth_w5500_seeed.conf` enables :kconfig:option:`CONFIG_ETH_W5500_POLL_MODE` to service the driver over SPI instead of the interrupt line.
Devicetree comes from the Zephyr shield devicetree :file:`zephyr/boards/shields/seeed_w5500/seeed_w5500.overlay`
plus a sample overlay: default :file:`w5500-seeed-static-mac.overlay` (fixed locally administered Ethernet MAC), or :file:`w5500-seeed.overlay` for ``zephyr,random-mac-address`` (new MAC each boot).

.. note::
   The sample :file:`w5500.overlay` is Arceli-specific (targets ``&eth_w5500_arceli_eth_w5500``).
   For ``seeed_w5500``, use :file:`w5500-seeed-static-mac.overlay` or :file:`w5500-seeed.overlay` (targets ``&eth_w5500``).

* From the sample directory (copy-paste each line):

  .. code-block:: console

     cd nrf/samples/dect/dect_tether_ipv6
     west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf" -DDTC_OVERLAY_FILE=w5500-seeed-static-mac.overlay

   * Edit ``local-mac-address`` in :file:`w5500-seeed-static-mac.overlay` so each board on the same LAN has a unique MAC.

   * For a **random** Ethernet MAC each boot, use :file:`w5500-seeed.overlay` instead:

     .. code-block:: console

        west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf" -DDTC_OVERLAY_FILE=w5500-seeed.overlay

Ethernet with ENC424J600 shield (Phytec link_board_eth)
======================================================

Use this when the host leg should use **Ethernet** via the Zephyr ``link_board_eth`` shield.
Merge :file:`pt.conf`, :file:`eth_common.conf`, and :file:`eth_link_board_eth.conf` (PT default).
Pin mapping and SPI node come from the Zephyr shield devicetree :file:`zephyr/boards/shields/link_board_eth/link_board_eth.overlay`.

* From the sample directory (copy-paste each line):

  .. code-block:: console

     cd nrf/samples/dect/dect_tether_ipv6
     west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=link_board_eth -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_link_board_eth.conf"

* Before attaching the shield to the DK, set VDD (nPM VOUT1) to 3.3 V.
* Connect the shield so shield pins are not shorted to other DK connections, attach it, then connect the RJ45 port to your LAN (router/switch).

.. _dect_tether_ipv6_dect_rx_pool:

DECT-private RX pool (:file:`dect_rx_pool.conf`)
================================================

Under sustained bidirectional load (PC → DECT uplink + downstream return traffic), eth0 RX bursts allocate from the global Zephyr pools (``CONFIG_NET_PKT_RX_COUNT`` / ``CONFIG_NET_BUF_RX_COUNT``) and can **starve dect0 RX**, surfacing as ``RX packet allocation failed in ISR`` drops, followed by ``nrf_modem_dect_dlc_data_tx returned NRF_ENOMEM`` on the TX side and ``eth_w5500: TX semaphore timeout`` on the sink.

Append :file:`dect_rx_pool.conf` **last** in ``EXTRA_CONF_FILE`` to enable ``CONFIG_DECT_MDM_RX_PRIVATE_POOL`` — a DECT-only ``net_pkt`` slab and ``net_buf`` pool isolated from the global RX pools:

.. code-block:: console

   cd nrf/samples/dect/dect_tether_ipv6
   west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf;dect_rx_pool.conf" -DDTC_OVERLAY_FILE=w5500-seeed-static-mac.overlay

Runtime inspection (with ``CONFIG_NET_BUF_POOL_USAGE=y`` and ``CONFIG_MEM_SLAB_TRACE_MAX_UTILIZATION=y`` already set by the overlay):

.. code-block:: console

   uart:~$ dect_mdm rx_pool
   DECT private RX pool:
   Address         Total   Free    MaxUsed Name
   0x...           35      35      8       dect_mdm_rx_pkts (slab)
   0x...           50      50      8       dect_mdm_rx_bufs (bufs, 128 B)

Bump ``PRIVATE_PKT_COUNT`` first if ``MaxUsed == Total`` under sustained traffic; raise ``PRIVATE_BUF_COUNT`` only if max-MTU downstream frames also become common (each consumes ``ceil(1500 / BUF_SIZE)`` fragments).
* ``CONFIG_DECT_MDM_NRF_DLC_SDU_LIFETIME=21`` — ``LIFETIME_1_5_S``: applied to the PT's ``flow_config[0]`` at association (the PT->FT direction). In the typical tether topology the PT sources data toward the internet, so **PT->FT is the heavy uplink**. 1.5 s is generous enough to absorb several DLC retransmission rounds across short outages without dropping queued payload, but small enough that a fundamentally broken link surfaces in seconds rather than the ~minute the 60 s default would impose. For latency-sensitive workloads where stale SDUs are useless, drop the value at runtime to e.g. ``=4`` (10 ms) or ``=6`` (30 ms). The FT counterpart in :ref:`dect_shell <dect_shell_dlc_resilient>` uses a slightly more tolerant ``=23`` (2.5 s) on its return path.

.. _dect_tether_ipv6_dlc_resilient:

Loss-resilient DLC profile (:file:`dlc_resilient.conf`)
=======================================================

The modem's stock DLC defaults — ``LIFETIME_60_S`` and "release association on DLC discard" — are not a great match for a tether deployment: stale SDUs can sit in the TX queue for up to 60 s, and a single discard-timer expiry tears down the association and forces a full PT reconnect. The :file:`dlc_resilient.conf` overlay replaces them with a loss-resilient profile:

* ``CONFIG_DECT_MDM_NRF_DLC_SDU_LIFETIME=21`` — ``LIFETIME_1_5_S``: applied to the PT's ``flow_config[0]`` at association (the PT->FT direction). In the typical tether topology the PT sources data toward the internet, so **PT->FT is the heavy uplink**. 1.5 s is generous enough to absorb several DLC retransmission rounds across short outages without dropping queued payload, but small enough that a fundamentally broken link surfaces in seconds rather than the ~minute the 60 s default would impose. For latency-sensitive workloads where stale SDUs are useless, drop the value at runtime to e.g. ``=4`` (10 ms) or ``=6`` (30 ms). The FT counterpart in :ref:`dect_shell <dect_shell_dlc_resilient>` uses a slightly more tolerant ``=23`` (2.5 s) on its return path.
* ``CONFIG_DECT_MDM_NRF_DLC_DISCARD_TIMER_RELEASE_ASSOCIATION=n`` — DLC-discard expiry no longer releases the association, so bursty loss does not cause link flaps and PT-side reconnect storms.

Append :file:`dlc_resilient.conf` **last** in ``EXTRA_CONF_FILE``:

.. code-block:: console

   cd nrf/samples/dect/dect_tether_ipv6
   west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf;dlc_resilient.conf" -DDTC_OVERLAY_FILE=w5500-seeed-static-mac.overlay

The overlay is composable with :file:`dect_rx_pool.conf` (append both, in any order):

.. code-block:: console

   west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=seeed_w5500 -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_w5500.conf;eth_w5500_seeed.conf;dect_rx_pool.conf;dlc_resilient.conf" -DDTC_OVERLAY_FILE=w5500-seeed-static-mac.overlay

Both knobs are also tunable at runtime via the DECT L2 shell, no rebuild required:

.. code-block:: console

   uart:~$ dect sett --dlc_sdu_lifetime 21
   uart:~$ dect sett --dlc_discard_release_assoc off
   uart:~$ dect sett --read

The ``--read`` output annotates which TX flow the lifetime applies to — ``PT->FT`` here (``PT`` role applies it to ``flow_config[0]`` at association). The FT-side **return path (FT->PT)** is controlled independently by the matching :file:`dlc_resilient.conf` in :ref:`dect_shell <dect_shell_dlc_resilient>`; pick each side's value for its own traffic profile.

Valid ``--dlc_sdu_lifetime`` values are ``1..31`` (0.5 ms .. 60 s, see ``enum dect_dlc_sdu_lifetime`` / ``nrf_modem_dect_dlc_sdu_lifetime``) or ``255`` for ``INFINITY``. Use ``31`` (60 s) to revert to the default at runtime.

mDNS / DNS-SD
=============

To advertise **``_dect-nr._udp``** (same service name as ``dect_shell``), merge overlays in this order: :file:`mdns-common.conf`, :file:`mdns.conf`, and (for Ethernet) :file:`eth_mdns.conf`; add :file:`mdns_forward.conf` last for the eth0 ↔ dect0 tap.

* From the sample directory (copy-paste each line):

  DECT-only (append to ``pt.conf`` or ``ft.conf``):

  .. code-block:: console

     cd nrf/samples/dect/dect_tether_ipv6
     west build -p -b nrf9151dk/nrf9151/ns -- -DEXTRA_CONF_FILE="pt.conf;mdns-common.conf;mdns.conf"

  Arceli W5500 — mDNS responder on ``eth0`` and ``dect0``:

  .. code-block:: console

     cd nrf/samples/dect/dect_tether_ipv6
     west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_w5500.conf;mdns-common.conf;mdns.conf;eth_mdns.conf" -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

  Arceli W5500 — add :file:`mdns_forward.conf` for the **eth0 ↔ dect0** mDNS tap in ``dect_tether_ipv6_lib`` (``CONFIG_DECT_TETHER_IPV6_MDNS_FORWARD``):

  .. code-block:: console

     cd nrf/samples/dect/dect_tether_ipv6
     west build -p -b nrf9151dk/nrf9151/ns -- -DSHIELD=arceli_eth_w5500 -DEXTRA_CONF_FILE="pt.conf;eth_common.conf;eth_w5500.conf;mdns-common.conf;mdns.conf;eth_mdns.conf;mdns_forward.conf" -DDTC_OVERLAY_FILE=w5500-static-mac.overlay

  Seeed W5500 and ``link_board_eth`` use the same mDNS conf chain; change ``SHIELD``, shield-specific conf, and ``DTC_OVERLAY_FILE`` as in the Ethernet sections above.

What this does:

* Zephyr **mDNS responder** listens on **UDP 5353** on each IPv6-capable interface (``eth0`` when present, and ``dect0``).
* :file:`src/mdns_dns_sd_listen.c` binds **UDP** port ``CONFIG_DECT_TETHER_IPV6_SAMPLE_MDNS_DNS_SD_PORT`` (default **4700**) on **``dect0``** so DNS-SD sees the advertised service port as in use, matching the ``dect_shell`` pattern.
* :file:`mdns_forward.conf` turns on ``CONFIG_DECT_TETHER_IPV6_MDNS_FORWARD`` (and ``CONFIG_NET_SOCKETS_PACKET``): an **AF_PACKET** tap in ``dect_tether_ipv6_lib`` (:file:`dect_tether_ipv6_mdns_fwd.c`) that forwards IPv6 mDNS between ``eth0`` and ``dect0`` **without IID NAT**. **Queries from Ethernet** are forwarded to DECT only when the IPv6 **source** is **ULA or GUA**; **fe80::** sources are skipped. **DECT toward Ethernet** forwards multicast to ``ff02::fb`` and **unicast** mDNS whose **destination** is ULA or GUA (so unicast replies to a DHCPv6 host can return on the tether).
Hostname / instance label defaults to ``CONFIG_NET_HOSTNAME`` in :file:`mdns.conf` (``dect-tether-ipv6``). Change there if several devices share a LAN.

.. _`ETSI TS 103 636-4`: https://www.etsi.org/deliver/etsi_ts/103600_103699/10363604/01.05.01_60/ts_10363604v010501p.pdf
