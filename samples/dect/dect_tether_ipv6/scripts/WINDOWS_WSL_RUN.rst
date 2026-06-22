Run ``mdns_query_dect.py`` from Windows (script in WSL)
=======================================================

The script defaults to **IPv6-only** mDNS and can pin traffic to **one interface**
(``--ifname eth0`` or ``--ipv6 fe80::…``).

Install ``zeroconf`` in the same environment that runs Python (WSL distro, not Windows,
unless you copy the script to Windows).

Find the interface name (WSL)
-----------------------------

In the WSL shell that has your NCS tree:

.. code-block:: console

   ip -br link
   ip -6 addr show dev eth0

Pick the interface that reaches your kit / LAN (often ``eth0`` in WSL2). For
``--ipv6``, use the link-local on that NIC if you prefer address-based bind.

Run entirely inside WSL (simplest)
----------------------------------

.. code-block:: console

   cd ~/ncs/nrf/samples/dect/dect_tether_ipv6/scripts
   python3 -m pip install -r requirements-mdns-query.txt
   python3 mdns_query_dect.py --ifname eth0 --timeout 15

From **Windows** (PowerShell or ``cmd``), invoke **WSL** so the script runs in Linux
with that distro’s network stack:

.. code-block:: powershell

   wsl -e bash -lc "cd ~/ncs/nrf/samples/dect/dect_tether_ipv6/scripts && python3 -m pip install -q -r requirements-mdns-query.txt && python3 mdns_query_dect.py --ifname eth0 --timeout 15"

Use ``-d DistroName`` if you have several distros:

.. code-block:: powershell

   wsl -d Ubuntu -e bash -lc "cd ~/ncs/nrf/samples/dect/dect_tether_ipv6/scripts && python3 mdns_query_dect.py --ifname eth0 --timeout 15"

If your home path differs, use the WSL path under ``\\wsl$\`` from Explorer to copy
the full ``cd`` target, e.g. ``\\wsl$\Ubuntu\home\<you>\ncs\...\scripts``.

WSL2 and mDNS on the physical LAN
---------------------------------

WSL2 uses a **virtual NAT** NIC by default; **multicast to your Ethernet/USB NIC on
the PC is often not visible** inside WSL, so mDNS to DECT peers on the LAN may fail
even with ``--ifname eth0``.

Mitigations:

1. **Windows 11 “mirrored networking” mode for WSL** (when available): WSL shares the
   host adapters more closely; try again with ``--ifname`` pointing at the interface
   that mirrors your tether.

2. **Run the script with Windows Python** instead: copy ``scripts\`` to the Windows
   side (or clone NCS on Windows), ``py -m pip install zeroconf``, then use Windows
   ``--ifname`` / binding options if you extend the script for Windows interface names.

3. **One-shot test from WSL** without LAN multicast: run the script on a Linux host
   that is actually on the same L2 as the kit.

Firewall
--------

Allow **UDP 5353** for the Python process (Windows Defender Firewall or WSL as
applicable).
