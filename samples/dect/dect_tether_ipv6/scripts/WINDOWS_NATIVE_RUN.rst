Run ``mdns_query_dect.py`` on Windows (copied from WSL)
======================================================

Use this when the nRF kit is on the **same Ethernet segment** as the PC’s **physical**
NIC (the one used with ``dect_bridge_routing`` / shield), and you want mDNS from
**Windows Python** (not WSL), so sockets use the real NIC.

1. Copy the script and requirements to Windows
-----------------------------------------------

From **PowerShell** (paths are examples — use your WSL user and distro name):

**Option A — File Explorer**

#. Open ``\\wsl$\Ubuntu\home\<you>\ncs\nrf\samples\dect\dect_tether_ipv6\scripts\``
#. Copy ``mdns_query_dect.py`` and ``requirements-mdns-query.txt`` to a Windows folder,
   e.g. ``C:\work\dect_mdns\``

**Option B — PowerShell + WSL path**

.. code-block:: powershell

   mkdir C:\work\dect_mdns -Force
   wsl -e bash -lc "cp ~/ncs/nrf/samples/dect/dect_tether_ipv6/scripts/mdns_query_dect.py /mnt/c/work/dect_mdns/"
   wsl -e bash -lc "cp ~/ncs/nrf/samples/dect/dect_tether_ipv6/scripts/requirements-mdns-query.txt /mnt/c/work/dect_mdns/"

2. Install Python dependency (Windows)
---------------------------------------

.. code-block:: powershell

   cd C:\work\dect_mdns
   py -m pip install -r requirements-mdns-query.txt

(Use ``python`` instead of ``py`` if that is how you invoke Python 3.)

3. Find the Ethernet adapter used for the tether
------------------------------------------------

The “routing bridge” host leg is the **NIC that goes to the nRF Ethernet shield**
(RJ45), not Wi‑Fi.

In **PowerShell**:

.. code-block:: powershell

   Get-NetAdapter | Where-Object Status -eq 'Up' | Format-Table Name, InterfaceDescription, ifIndex

Note the **Name** (e.g. ``Ethernet``, ``Ethernet 2``) or **ifIndex**.

Then list **IPv6** addresses on that adapter (replace ``12`` with your ``ifIndex``):

.. code-block:: powershell

   Get-NetIPAddress -AddressFamily IPv6 -InterfaceIndex 12 | Format-Table IPAddress, PrefixOrigin

Use the **link-local** ``fe80::…`` row for this adapter (often ``PrefixOrigin`` Link or RouterAdvertisement).

4. Run the script (IPv6-only, pinned to that NIC)
--------------------------------------------------

**Recommended on Windows:** bind with ``--ipv6`` using that link-local (Zeroconf binds the socket to that address):

.. code-block:: powershell

   cd C:\work\dect_mdns
   py mdns_query_dect.py --ipv6 fe80::1234:5678:9abc:def0%12 --timeout 15

Replace ``fe80::…`` with the value from ``Get-NetIPAddress``. The ``%12`` **zone index**
is the **ifIndex**; it helps Windows pick the correct interface for link-local scope.

If binding without zone fails, try the same address **with** ``%<ifIndex>`` as above.

**Alternative:** try interface **name** (Python 3.8+ on Windows may support it):

.. code-block:: powershell

   py mdns_query_dect.py --ifname "Ethernet" --timeout 15

If ``--ifname`` errors, use ``--ipv6`` with ``fe80::…%ifIndex`` instead.

5. Firewall
-----------

Allow **UDP 5353** for **Private** networks when Windows prompts, or add an inbound
rule for your Python executable.

.. important::

   The ``py.exe`` **launcher** (e.g. ``C:\Users\<you>\AppData\Local\Microsoft\WindowsApps\py.exe``)
   is only a shim that re-execs the real interpreter. Windows firewall rules apply to
   the **process that owns the socket**, which is the actual interpreter binary, **not**
   the launcher. A rule targeted at the launcher path silently has no effect — Wireshark
   sees the unicast mDNS reply, but ``recv`` stays empty.

   Discover the real interpreter path:

   .. code-block:: powershell

      py -c "import sys; print(sys.executable)"

   Typical result on Python 3.14: ``C:\Users\<you>\AppData\Local\Python\pythoncore-3.14-64\python.exe``.

   Then add the rule against **that** path (run elevated):

   .. code-block:: powershell

      New-NetFirewallRule -DisplayName "mDNS Test - Python core inbound UDP" `
        -Direction Inbound -Action Allow `
        -Program "C:\Users\<you>\AppData\Local\Python\pythoncore-3.14-64\python.exe" `
        -Protocol UDP -Profile Any

   Verify:

   .. code-block:: powershell

      Get-NetFirewallRule -DisplayName "mDNS Test*" |
        Format-List DisplayName, Enabled

   Re-run the script (normal, non-elevated PowerShell) and expect ``Done (N datagram(s))``
   with ``N >= 1``.

   **Cleanup (remove every rule created here).** If you first tried a rule against the
   ``py.exe`` launcher path before discovering it had no effect, you likely have **two**
   rules now (e.g. ``mDNS Test - Python inbound UDP`` for the launcher and
   ``mDNS Test - Python core inbound UDP`` for the real interpreter). The wildcard below
   removes **both** in one shot — run elevated:

   .. code-block:: powershell

      Get-NetFirewallRule -DisplayName "mDNS Test*" |
        Format-List DisplayName, Enabled
      Remove-NetFirewallRule -DisplayName "mDNS Test*"
      Get-NetFirewallRule -DisplayName "mDNS Test*" -ErrorAction SilentlyContinue

6. Defaults
-----------

The script defaults to **IPv6-only** (``--ip-version 6``). The service type is
``_dect-nr._udp.local.`` unless you override ``--types``.

7. Zeroconf shows nothing but Wireshark shows mDNS replies
----------------------------------------------------------

On Windows, ``zeroconf`` may not join ``ff02::fb`` correctly when the socket is tied
to one adapter, so **browse callbacks never run** even though Wireshark shows
**Standard query response** on that NIC.

Use **raw PTR** mode (sends a PTR query and **explicitly joins** ``ff02::fb`` on
your ``%ifIndex`` scope, then prints every UDP/5353 payload). Replies are decoded
with **dnslib** (mDNS cache-flush ``CLASS`` values do not break the parser).

.. code-block:: powershell

   cd C:\work\dect_mdns
   py -m pip install -r requirements-mdns-query.txt
   py mdns_query_dect.py --raw-ptr --ipv6 "fe80::YOUR:LINK:LOCAL%12" --timeout 20

Optional Zeroconf debug (normal mode only):

.. code-block:: powershell

   py mdns_query_dect.py --ipv6 "fe80::…%12" --timeout 15 --zeroconf-debug

8. No UDP/5353 replies when binding ULA (raw-ptr)
------------------------------------------------

There is **no NAT** of the query: the script sends plain mDNS to **``ff02::fb``**
(link-local multicast scope) on the **same L2 segment** as the NIC you bound.

* A **ULA or GUA** on ``--ipv6`` only sets the **source address** for that socket
  (Windows uses ``sin6_scope_id=0`` on bind for non–link-local; see script
  docstring). It does **not** forward the packet onto DECT.
* If ``Ethernet`` is your **office LAN** (DHCP, DNS, ``fdde:…`` from RA) and the
  nRF kit is on a **different** cable or VLAN, mDNS never reaches the tether gateway —
  ``Done (0 datagram(s))`` is expected.
* Logs on the **FT** (``dect0``, ``fe80::…`` peers) are often **mesh-only** traffic;
  they are not proof that your PC’s Ethernet frame arrived on DECT.

**Fix:** use the **link-local** ``fe80::…%<ifIndex>`` on the **RJ45 NIC that is on
the same Ethernet segment as the nRF shield** (sections 3–4 and raw-ptr example
above). If you must use a global/ULA bind for unicast return path, you still need
that NIC to be the segment where responders live.

When the nRF runs **dect_tether_ipv6** with ``eth_common.conf``,
``mdns-common.conf``, ``mdns.conf``, ``eth_mdns.conf``, and
``mdns_forward.conf`` (SPI Ethernet shield),
``CONFIG_DECT_TETHER_IPV6_MDNS_FORWARD`` forwards **ff02::fb** mDNS between that
**tether** and **dect0** without IID rewrite. That path applies only to the
**shield cable**, not your office ``Ethernet`` adapter.

9. Wireshark shows mDNS but ``mdns_query_dect.py`` prints ``Done (0 datagram(s))``
-------------------------------------------------------------------------------

Wireshark captures **before** the Windows firewall and independent of which process
owns the socket. **Unicast** mDNS replies must hit the **exact** IPv6 address and **UDP
port** your Python socket bound (the script prints the bound tuple after ``bind`` —
compare the Wireshark **Destination** column on response rows).

* **Stable vs temporary GUA:** Windows may send the query from one address while
  responders send answers to your **stable** SLAAC address (or the opposite). Bind
  ``--ipv6`` to the same address Wireshark shows as **Destination** on the reply.
* **Firewall:** allow **inbound UDP** for ``python.exe`` (or your venv interpreter) on
  **Private** networks; set the adapter profile to Private so the rule applies.
* **Several AAAA records** in the capture (link-local, ULA, global) only mean the device
  answered fully; **recv** on the PC can still be empty when the query source was a GUA
  and the OS/firewall did not deliver unicast to the ephemeral port — prefer
  ``--raw-ptr --ipv6 "fe80::…%<ifIndex>"`` (section 8) for a path that usually matches
  Wireshark and Python.
* The script sets **``SO_REUSEADDR``** to coexist with Bonjour where the OS allows it.
