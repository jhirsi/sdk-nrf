# DECT IPv6 tethering (dect_tether_ipv6) — engineering analysis

This note gathers design and operational points discussed for
`dect_tether_ipv6` (Zephyr/NCS). It is not a user-facing tutorial; see
sample `README.rst` under `nrf/samples/dect/dect_tether_ipv6/` for usage.

---

## 1. Architecture: routing, not L2 hooks

**Goal:** Tethered PC on Ethernet gets ULA/GUA via DHCPv6; DECT toward the
parent carries tunneled/routed IPv6 without relying on
`CONFIG_NET_L2_DECT_IPV6_PREPARE_HOOKS` for the PC’s global addresses on
`dect0`.

**Mechanism (`CONFIG_DECT_TETHER_IPV6_FWD`):**

- **DECT leg:** Default IPv6 router on the DECT interface toward the **parent**
  after parent association.
- **Ethernet leg:** After DHCPv6 offers ULA/GUA, install **`/128` routes** and
  **neighbor entries** on the first Ethernet interface so forwarded traffic
  reaches the PC.
- **Skipping duplicate unicast on `dect0`:** With
  `CONFIG_NET_L2_DECT_IPV6_IFACE_UNICAST_SKIP`, ULA/GUA may not appear in the
  normal iface address list; code uses Nordic L2 helpers (e.g.
  `dect_net_l2_ipv6_off_iface_unicast_get()`) where needed.

Forwarding is **normal Zephyr IPv6 routing** (routes + ND), not L2 address
rewrite hooks.

---

## 2. Host leg: RA and DHCPv6

- **RA (Ethernet):** Minimal Router Advertisements — default router, optional
  MTU, **Managed (M)** when enabled, optional **RDNSS** (RFC 8106).
- **DHCPv6:** Server on UDP **547**, joins **`ff02::1:2`** so clients using the
  all-DHCP-relays/agents multicast are answered quickly.

**Reference:** `dect_bridge_routing` host RA behavior (mgmt-driven RA, DECT
iface sync) was used as a conceptual reference; host IPv6 RA/DHCP live under
`dect_tether_ipv6/`.

---

## 3. DECT disconnect: inform the PC

When the **parent association is released** (handled in
`dect_tether_ipv6_fwd.c` via `NET_EVENT_DECT_ASSOCIATION_CHANGED`):

1. **DHCPv6:** Queue unsolicited **Reply** message(s) with **IA_NA / IAADDR
   preferred and valid lifetimes 0** for committed leases (Request/Renew
   Reply). The server keeps a **stale** copy of the previous ULA/GUA for the
   same client DUID+IAID when addresses change (e.g. after `renew6`), so
   **both** generations can be revoked on disconnect. Sends run only from the
   DHCP server thread (same UDP socket).
2. **RA:** **`dect_tether_ipv6_ra_kick()`** sends **one** unsolicited RA
   (router/DNS depref when uplink is gone). Uplink is derived from
   **`dect_net_l2_parent_ipv6_addr_get()`**. If there is **no** parent IPv6
   address: **router lifetime 0**, **M cleared**, **RDNSS lifetime 0** (DNS
   withdrawal when RDNSS is enabled).

**Association create:** RA is kicked again after refreshing the DECT default
router so the PC sees a valid router/DNS when the link returns.

**Dependency:** Disconnect hooks are wired from **`dect_tether_ipv6_fwd`**
If `CONFIG_DECT_TETHER_IPV6_FWD` is disabled, another mgmt subscriber
would be needed for the same RA/DHCP behavior.

---

## 4. DECT L2 shell

`CONFIG_DECT_L2_SHELL_LIB` **depends on `CONFIG_GETOPT_LONG`** (see
`l2_shell/Kconfig`). Samples enabling the L2 shell must set **`GETOPT_LONG=y`**
or the option is silently unsatisfied and the shell library is not built.

---

## 5. DNS and traffic path (practical picture)

- **RDNSS in RA** tells the PC **which resolver address** to use (e.g. a public
  DNS if configured).
- **Outbound** (e.g. DNS query): **src** = PC ULA/GUA, **dst** = resolver;
  default route on the PC points at the tether gateway’s Ethernet LL; the nRF **forwards**
  using the **default route on DECT** toward the parent, then upstream IPv6.
- **Return traffic:** **dst** = PC address; arrives via parent → DECT → nRF;
  **`/128` + neighbor** on Ethernet delivers to the PC.

**Caveat:** A **ULA-only** PC talking to a **global** RDNSS address may not have
a viable return path on the open Internet. Deployments often use **GUA** on the
PC, on-link DNS, or DNS reachable in the same scope as the PC’s addresses.

---

## 6. Windows: duplicate IPv6 after `ipconfig /renew6`

**Symptom:** Two GUA and two ULA pairs (different interface IDs), both
“Preferred”, same DNS.

**Cause:** Windows **adds** new DHCPv6 bindings when the server returns **new**
IA_NA addresses; it **keeps** old addresses until **valid lifetime** expires
unless the server sets **old IAADDR lifetimes to 0** in the same exchange.

The tether gateway currently reads **current** DECT-derived ULA/GUA on each message; if
those change between exchanges, **renew** can produce a **new** pair while the
**old** pair is still valid on the host.

**Mitigations (operational):** Disable/enable adapter, `ipconfig /release6` /
`renew6`, or reboot. **Mitigations (firmware):** On Renew, return **stable**
addresses for the same DUID+IAID when DECT has not changed, or explicitly
**revoke** old IAADDR in the Reply when issuing new ones.

---

## 7. Ethernet IPv6 source filtering (future / design note)

**Idea:** Drop IPv6 packets received on the **Ethernet** leg whose **source**
is not allowed — e.g. not the **ULA/GUA offered via DHCPv6** (and typically still
allow **link-local** for ND/DHCPv6).

**Zephyr mechanism:** `CONFIG_NET_PKT_FILTER` and
`CONFIG_NET_PKT_FILTER_IPV6_HOOK`. IPv6 input calls
`net_pkt_filter_ip_recv_ok()` early in `net_ipv6_input()`. Rules live in
`npf_ipv6_recv_rules` (`zephyr/subsys/net/pkt_filter/`).

**Design constraints:**

- Rules apply to **all** IPv6 input; the first rules should **accept** traffic
  that **did not** originate on the tethered Ethernet (e.g. DECT), then apply
  strict logic only for the PC leg.
- If the IPv6 rule list is **non-empty** and **no** rule matches, the default
  outcome is **drop** — append **`npf_default_ok`** or an explicit catch-all
  **NET_OK** rule.
- **State:** Reuse the same **committed DHCP lease** (ULA/GUA) as the allowlist;
  clear on disconnect/revoke.
- **Optional hardening:** Combine with **Ethernet source MAC** checks if the
  NPF path exposes them for the same packets.

**Not implemented** in tree at the time of this document; keep as a tracked
enhancement if product security requires it.

---

## 8. File map (implementation)

| Area | Main files |
|------|------------|
| Init order | `dect_tether_ipv6.c` |
| RA | `dect_tether_ipv6_ra.c` |
| DHCPv6 + lease snapshot / revoke | `dect_tether_ipv6_dhcpv6_srv.c` |
| DECT parent router, tether routes, mgmt | `dect_tether_ipv6_fwd.c` |
| Address pick / L2 skip | `dect_tether_ipv6_addr.c`, `dect_net_l2_ipv6.*` |
| Kconfig | `dect_tether_ipv6/Kconfig` |

---

## 9. mDNS between Ethernet (PC) and DECT

See **`dect_mdns_eth_dect_forwarding_analysis.md`** in this directory for a
dedicated analysis (RFC 6762 scope, AF_PACKET tap forwarder, optional IID
masquerade, and how to reuse or port the routing-bridge implementation toward
`dect_tether_ipv6`).

---

## 10. W5500 (Arceli shield): link flap and `TX: tx_sem timeout (10 ms)`

**Symptoms:** `eth_w5500: Link down` / `Link up`, then bursts of
`TX: tx_sem timeout (10 ms) len=… — IRQ thread may be late` and
`net_if: … send failure status -5` (`-EIO`).

**Driver behavior (Zephyr `eth_w5500.c`):**

- **`w5500_tx()`** issues `SEND`, then **`k_sem_take(&tx_sem, K_MSEC(10))`**. The
  semaphore is released only when the **`eth_w5500`** thread reads socket IR
  **`S0_IR_SENDOK`** after the **INT GPIO** path wakes it (`int_sem`).
- If that thread runs **later than 10 ms** (SPI service time, long RX drain,
  higher-priority work, or a **missed / delayed INT**), TX fails with the log
  you see. This is **not** the same as `CONFIG_ETH_W5500_TIMEOUT` (RX buffer
  alloc); the **10 ms TX wait is hardcoded** in the driver.

**Priority note:** `K_PRIO_COOP(x) = -(NUM_COOP_PRIORITIES - x)`. **Smaller
`x` ⇒ more negative scheduler priority ⇒ higher cooperative priority.** So
**`CONFIG_ETH_W5500_RX_THREAD_PRIO=2` (driver default) is higher than `8` or
`15`**. Values like **8** or **15** actually **lower** the IRQ thread relative to
other coop threads — a common mistake. To prioritize the W5500 thread, use
**`0`** or **`1`** (subject to `0 … NUM_COOP_PRIORITIES-1`).

**Mitigations (try in order):**

1. **`CONFIG_ETH_W5500_RX_THREAD_PRIO`:** use **`0`** or **`1`** if SENDOK is
   late; keep **`CONFIG_ETH_W5500_RX_THREAD_STACK_SIZE`** generous (sample
   overlay already uses 3072).
2. **SPI:** faster clock (DT), **dedicated SPI** / short wires; avoid sharing the
   bus with long blocking transfers during heavy TX (e.g. right after link-up
   when RA + DHCPv6 + ND burst).
3. **Hardware / PHY:** treat **link down/up** as a separate issue (cable, dock,
   switch EEE, shield power). Flapping link causes **carrier off/on** and
   **TX storms** on rejoin.
4. **Upstream / local patch:** make the TX semaphore timeout **Kconfigurable**
   (e.g. 50–100 ms) or **poll `S0_IR` once** if the sem times out, if IRQ edges
   are occasionally lost.

---

## Document history

- **2026-04-30:** Initial consolidation from design discussion (routing model,
  RA/DHCPv6, disconnect signaling, shell Kconfig, DNS path, Windows renew
  behavior, NPF-based source filter idea).
- **2026-04-30:** Section 9 pointer — mDNS eth↔DECT forwarding analysis doc.
- **2026-04-30:** Section 10 — W5500 `tx_sem` / link stability notes.
