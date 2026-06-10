# mDNS / DNS-SD between Ethernet (PC) and DECT — analysis

**Goal (user story):** From a tethered **PC on Ethernet**, run mDNS queries and
service discovery against **devices on the DECT leg**, and allow **responses**
(and optionally announcements) to reach the PC. Traffic must flow **both
directions** (Ethernet ↔ DECT).

This document explains **why normal IPv6 routing is not enough**, what Nordic
already ships for the **routing bridge**, and **implementation options** for
`dect_tether_ipv6` (routing-only IPv6 tether gateway, no L2 prepare hooks).

---

## 1. What mDNS is (relevant subset)

- **Transport:** UDP port **5353** (RFC 6762).
- **IPv4:** multicast **`224.0.0.251`**, link-local scope.
- **IPv6:** multicast **`ff02::fb`**, link-local scope (UDP 5353).

mDNS assumes a **single link** for link-scoped multicast. Crossing **Ethernet**
and **DECT** is **not** transparent bridging of Ethernet frames; you need a
**reflector / proxy** behavior at the nRF.

---

## 2. Why unicast/routing alone does not solve discovery

1. **Multicast does not “route” like unicast GUA:** Link-local multicast is not
   forwarded across interfaces by typical host/router rules unless something
   explicitly **replicates** or **proxies** it.
2. **MLD / join state:** Receivers join **`ff02::fb`** on each link. The bridge
   must ensure **both** Ethernet and DECT stacks (or the tap path) actually
   **receive** mDNS frames. On DECT, Nordic L2 may join mDNS when appropriate
   (see e.g. `dect_net_l2_join_ipv6_mdns_group()` usage in PPP sample comments).
3. **Responder behavior:** Many stacks expect the **query source** to be
   **link-local** or at least **on-link** for the DECT segment. A query arriving
   on DECT with the PC’s **Ethernet GUA** as source can be **ignored** or
   answered toward an address DECT devices do not route back.

So a practical bridge usually does more than “copy UDP 5353”: it often
**rewrites IPv6 source (or destination on return)** in a controlled way.

---

## 3. Reference implementation in NCS (routing bridge)

The **`dect_bridge_routing`** library includes an optional **link-layer
multicast forwarder** for **IPv6 mDNS**:

| Piece | Location |
|-------|-----------|
| Forwarder thread, AF_PACKET taps, filters, TX | `dect_bridge_routing_ll_mcast.c` |
| Kconfig | `DECT_BRIDGE_ROUTING_LL_MULTICAST_FORWARD`, `…_MDNS_NAT`, stack/prio, MRU size |
| Start/stop wiring | `dect_bridge_routing.c` (`dect_bridge_routing_ll_mcast_start/stop`) |
| Sample overlay | `nrf/samples/dect/dect_bridge_routing/mdns.conf` |

**Mechanism (summary):**

1. **`CONFIG_NET_SOCKETS_PACKET`:** Open **`AF_PACKET`** / `SOCK_DGRAM` sockets
   bound to **Ethernet** and **DECT** interface indexes (`ETH_P_ALL`).
2. **Poll loop:** On read from **Ethernet**, accept **IPv6 UDP mDNS** packets
   whose destination is **`ff02::fb`** (query path to multicast group). Forward
   to DECT by building a **`net_pkt`** and **`net_if_send_data()`** on the DECT
   interface (same pattern as PPP bridge style TX).
3. **DECT → Ethernet:** Accept IPv6 UDP **5353** if destination is **`ff02::fb`**
   **or** (with NAT) certain **unicast** mDNS reply patterns keyed off the
   masquerade mapping.
4. **Optional `DECT_BRIDGE_ROUTING_LL_MULTICAST_MDNS_NAT`:** On **Ethernet →
   DECT**, rewrite the IPv6 **source IID** (lower 64 bits) to match **`dect0`**
   link-local IID while keeping the **upper /64** (so GUA/ULA/fe80 prefix style
   is preserved). Maintain an **MRU table** (masqueraded source + UDP ephemeral
   → original PC address) for **unicast** replies. **UDP checksum** is updated
   (RFC 1624 incremental style in-tree).

**IPv4 mDNS:** The current `dect_bridge_routing_ll_mcast.c` path is **IPv6
only** (`NET_AF_INET6` in TX). If the PC uses **IPv4-only mDNS** (`224.0.0.251`),
a **parallel IPv4 branch** (parse IPv4 header, same tap idea, IGMP joins) would
be a separate extension.

**Integration with L2 prepare hooks:** When `…_LL_MULTICAST_MDNS_NAT` is enabled,
`dect_bridge_routing.c` also syncs the MRU from **TX/RX prepare** paths for mDNS
UDP that flows through the **stack** (not only the AF_PACKET tap). The host
IPv6 bridge **does not use** those prepare hooks today; the **tap-only NAT path**
inside `dect_bridge_routing_ll_mcast.c` is still the core for PC ↔ DECT mDNS
**as long as all mDNS user traffic goes through the tap-forwarded path**.

---

## 4. Mapping this onto `dect_tether_ipv6`

`dect_tether_ipv6` uses **Zephyr routing** (default router on DECT,
`/128` + neighbor on Ethernet) and **does not** enable the routing bridge’s
IPv6 prepare hooks. To add “PC discovers DECT over mDNS”, you have **three
structural options**:

### Option A — Shared library module (recommended long-term)

1. **Extract** `dect_bridge_routing_ll_mcast.c` (and its Kconfig fragments) into a
   small shared library under e.g. `nrf/subsys/net/lib/dect/dect_bridge_ll_mcast/`
   (name TBD), depending only on **Ethernet + DECT + PACKET sockets**, not on
   `dect_bridge_routing`’s prepare-hook API.
2. **`dect_bridge_routing`:** Thin wrapper + optional **MRU sync** from prepare
   hooks remains in routing only.
3. **`dect_tether_ipv6`:** New Kconfig e.g.
   `DECT_TETHER_IPV6_MDNS_FORWARD` → `select NET_SOCKETS_PACKET`, bump
   `NET_IF_MCAST_IPV6_ADDR_COUNT` as in `mdns.conf`, call shared
   `…_ll_mcast_start/stop` from `dect_tether_ipv6_init/deinit` (or sample).

**Pros:** One implementation, consistent behavior, easier testing.
**Cons:** Refactor + ABI/Kconfig churn.

### Option B — Copy-adapt into `dect_tether_ipv6`

Duplicate the tap thread with renamed Kconfig symbols under
`dect_tether_ipv6/`. Wire start/stop next to RA/DHCP init.

**Pros:** No dependency from host lib on routing lib.
**Cons:** Two copies of subtle NAT/checksum/MRU logic.

### Option C — Kconfig `select` routing bridge multicast from host sample only

Only the **sample** enables both `DECT_TETHER_IPV6_LIB` and routing’s
`DECT_BRIDGE_ROUTING_LL_MULTICAST_*` without linking full routing semantics —
**not viable** if routing Kconfig forces other routing-only symbols.

**Verdict:** Prefer **A** or **B**; avoid **C** unless Kconfig is carefully split.

---

## 5. Kconfig and resource checklist (Zephyr/NCS)

When enabling the forwarder (matches routing sample overlay patterns):

- `CONFIG_NET_SOCKETS_PACKET=y`
- Sufficient **`CONFIG_NET_IF_MCAST_IPV6_ADDR_COUNT`** on both legs (mDNS +
  ND + RA listeners).
- If the device also runs **`CONFIG_MDNS_RESPONDER`**, raise
  **`CONFIG_NET_SOCKETS_SERVICE_STACK_SIZE`** (routing `mdns.conf`
  documents stack pressure under bursts).
- Optional: `CONFIG_DNS_SD`, `CONFIG_MDNS_RESPONDER_DNS_SD` if the **nRF itself**
  should advertise on DECT (orthogonal to forwarding the **PC’s** queries).

---

## 6. Operational caveats

- **Service names / collision:** Discovery works across the bridge; **uniqueness**
  of instance names is still end-to-end (PC vs DECT namespace).
- **Multiple PCs on Ethernet:** MRU NAT table size (`…_MDNS_NAT_MAP_SIZE`) and
  IID disambiguation via **UDP ephemeral** (see comments in
  `dect_bridge_routing_ll_mcast.c`) matter under load.
- **Security:** A tap forwarder extends link-local discovery across an **L2
  boundary**; treat as a **trusted tether** model unless combined with future
  **source filtering** (see `dect_tether_ipv6_analysis.md`).

---

## 7. Suggested next implementation steps

1. Prototype **Option B** behind `DECT_TETHER_IPV6_MDNS_FORWARD` in the
   host library + sample `mdns.conf` mirroring routing’s overlay.
2. Validate **PC → DECT** `_services._dns-sd._udp` query and **unicast** reply
   path with NAT on/off.
3. Refactor to **Option A** once behavior is stable.

---

## Document history

- **2026-04-30:** Initial analysis (RFC 6762, routing bridge reference, host_ipv6
  integration options, IPv4 gap, Kconfig checklist).
