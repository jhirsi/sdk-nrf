#!/usr/bin/env python3
# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
"""
mDNS browse (IPv6-only by default) for Nordic DECT DNS-SD services.

Service types (same as ``dect_discover_shell.c``):
  _dect-nr._udp.local

Install (inside WSL or on Windows Python):
  python3 -m pip install -r requirements-mdns-query.txt

Examples:
  python3 mdns_query_dect.py --ifname eth0 --timeout 10
  python3 mdns_query_dect.py --ipv6 fe80::abcd:1ff:fe00:1%eth0 --timeout 10
  py mdns_query_dect.py --ipv6 "2001:db8::1%4" --timeout 15   # Zeroconf: scoped addr%zone (not bare ifindex)

If Zeroconf shows nothing but Wireshark shows mDNS on the NIC (including **multicast**
``ff02::fb`` UDP **5353**), Windows Firewall often blocks **inbound** mDNS to **python.exe**
for both multicast and unicast — allow **UDP 5353** (Private profile) or use **--raw-ptr**
with **link-local** ``--ipv6``. Use **raw PTR** mode
(join ``ff02::fb`` on that interface; decodes replies with **dnslib** when possible.
**dnslib** rejects empty TXT (``RDLENGTH == 0``), which is valid in mDNS — the script
then prints a **loose** wire summary instead):

  py mdns_query_dect.py --raw-ptr --ipv6 "fe80::1234:5678:9abc:def0%12" --timeout 15
  py mdns_query_dect.py --raw-ptr --ipv6 "2001:db8::1%4" --timeout 15   # global/ULA: use %ifIndex on Windows

On **Windows**, mDNS replies are often **unicast** to the query’s **IPv6 source** and UDP
ephemeral port. For **ULA/GUA** with ``%<InterfaceIndex>``, **raw-ptr** prefers
``bind("::", …, ifindex)`` first so unicasts to any address on that interface (including
your ``--ipv6`` ULA) demux to the same ephemeral port — a narrow bind to ``ADDR`` alone
often shows replies in Wireshark but **recv** stays empty. If ``::`` bind fails, it falls
back to ``ADDR`` with ``sin6_scope_id`` **0** then **ifindex** (link-local still uses
``fe80`` + zone only). It pins the query source with **sendmsg(IPV6_PKTINFO)** or, when
the stdlib omits ``socket.sendmsg``, **WSASendMsg** via ``ctypes``. For any **scoped**
non–link-local bind (``%`` ifindex; **ULA or GUA**), a second quiet probe is sent at
**+250 ms** on **all** OSes while in the receive loop (``select``-based waits). After
each wake, **all** queued datagrams are drained so burst mDNS is not dropped.
**fe80::** uses link-local bind + ``sendto``. Outgoing multicast uses **Hop Limit 255**
(``IPV6_MULTICAST_HOPS``) per RFC 6762 — without it, many stacks ignore the query.
**Get-NetIPAddress**
may infer ``%`` when omitted. Set **MDNS_RAW_PTR_DEBUG** for extra bind/send diagnostics
and zero-reply troubleshooting text after ``Done (0 …)``.

**Wireshark shows replies but this script prints ``Done (0 datagram(s))``:** compare the
query frame’s **IPv6 Source** to ``--ipv6``; capture on the **same** adapter you passed as
``%`` index. If you see **no** answers when using a **ULA** source, confirm that ULA is
assigned on that interface and that the query (dst ``ff02::fb``, UDP 5353) appears on the
capture — otherwise the issue is outside this script (routing / RA / bridge).

Updated DECT firmware (with ``CONFIG_MDNS_RESPONDER_DNS_SD_MULTIPLE_AAAA=y``, as in the
sample ``mdns-discover.conf`` / ``overlay-mdns.conf``) often answers PTR with **several
AAAA records** (link-local, ULA, global on the same interface). The script does not need
changes for that; it still needs the **UDP reply** to
reach the bound socket. On **Windows**, **link-local** ``--ipv6 "fe80::…%ifIndex"`` plus a
firewall rule for **python.exe** inbound UDP is still the most reliable way to see
``recv`` succeed when captures show unicast to an ephemeral port.

On **Windows**, if Zeroconf finds nothing but **--ipv6** was set, the script runs a short
**--raw-ptr** fallback for the first ``--types`` entry (disable with **--no-raw-ptr-fallback**).

Docs: ``WINDOWS_NATIVE_RUN.rst``, ``WINDOWS_WSL_RUN.rst`` in this folder.
"""

from __future__ import annotations

import argparse
import ipaddress
import logging
import os
import select
import socket
import struct
import subprocess
import sys
import time
from typing import Optional

try:
    from zeroconf import IPVersion, ServiceBrowser, ServiceInfo, ServiceListener, Zeroconf
except ImportError:
    print("Missing dependency. Install with:\n  python3 -m pip install zeroconf", file=sys.stderr)
    sys.exit(1)


def _ipv6_is_link_local(addr: str) -> bool:
    try:
        return ipaddress.IPv6Address(addr).is_link_local
    except ValueError:
        return addr.lower().startswith("fe80:")


def _ipv6_scope_kind(addr: str) -> str:
    """``ll`` | ``ula`` | ``gua`` | ``other`` for diagnostics (ignores ``%zone``)."""
    base = addr.split("%", 1)[0].strip()
    try:
        a = ipaddress.IPv6Address(base)
    except ValueError:
        return "other"
    if a.is_link_local:
        return "ll"
    if a.is_private:
        return "ula"
    if a.is_global:
        return "gua"
    return "other"


def _windows_ipv6_bind_scope_id(addr: str, zone_ifindex: int) -> int:
    """
    Windows: bind() rejects non-link-local addresses with non-zero sin6_scope_id (10049).
    Interface index still applies to IPV6_JOIN_GROUP and link-scope multicast sendto.
    """
    if sys.platform != "win32":
        return zone_ifindex
    if zone_ifindex <= 0:
        return 0
    return zone_ifindex if _ipv6_is_link_local(addr) else 0


def _parse_ipv6_bind(s: str) -> tuple[str, int]:
    """
    Parse ``fe80::1`` or ``fe80::1%12`` / ``fe80::1%eth0`` into (addr, scope_id).
    Scope 0 means unspecified (may fail for link-local bind on some OS).
    """
    s = s.strip()
    if "%" not in s:
        return s, 0
    addr, zone = s.rsplit("%", 1)
    zone = zone.strip()
    if zone.isdigit():
        return addr, int(zone)
    try:
        return addr, socket.if_nametoindex(zone)
    except OSError as e:
        raise ValueError(f"cannot resolve zone {zone!r}: {e}") from e


def _windows_interface_index_for_bound_ipv6(addr: str) -> int:
    """
    Return the Windows interface index where ``addr`` is assigned (Get-NetIPAddress),
    or 0 if unknown. Needed so unicast mDNS replies are demuxed to the bound UDP socket.
    """
    if sys.platform != "win32":
        return 0
    try:
        ipaddress.IPv6Address(addr)
    except ValueError:
        return 0
    safe = addr.replace("'", "''")
    ps = (
        "(Get-NetIPAddress -AddressFamily IPv6 | "
        f"Where-Object {{ $_.IPAddress -eq '{safe}' }}).InterfaceIndex"
    )
    try:
        cp = subprocess.run(
            ["powershell", "-NoProfile", "-Command", ps],
            capture_output=True,
            text=True,
            timeout=8,
            check=False,
        )
        if cp.returncode != 0:
            return 0
        for line in cp.stdout.strip().splitlines():
            line = line.strip()
            if line.isdigit():
                return int(line)
    except (OSError, subprocess.TimeoutExpired, ValueError):
        pass
    return 0


def _ipv6_in6_pktinfo_bytes(addr: str, ifindex: int) -> bytes:
    """RFC 3542 / IN6_PKTINFO: 128-bit address + interface index (for IPV6_PKTINFO cmsg)."""
    return socket.inet_pton(socket.AF_INET6, addr) + struct.pack("@I", ifindex)


def _ipv6_pktinfo_cmsg_type() -> int:
    t = getattr(socket, "IPV6_PKTINFO", None)
    if t is not None:
        return int(t)
    # CPython usually defines this; Winsock historically used 19, Linux 50.
    return 19 if sys.platform == "win32" else 50


def _win_sendmsg_supported(sock: socket.socket) -> bool:
    return callable(getattr(sock, "sendmsg", None))


_win_wsasendmsg_cache: object | None = None


def _raw_ptr_debug() -> bool:
    return os.environ.get("MDNS_RAW_PTR_DEBUG", "").strip().lower() in (
        "1",
        "true",
        "yes",
        "on",
    )


def _win_wsasendmsg_load() -> dict | bool:
    """
    Load ws2_32.WSASendMsg + WSAMSG/WSABUF for IPv6 ancillary send.
    Returns False if Winsock/ctypes setup fails (caller falls back to sendto).
    """
    import ctypes
    from ctypes import POINTER, Structure, byref, wintypes

    class WSABUF(Structure):
        _pack_ = 8
        _fields_ = [("len", wintypes.ULONG), ("buf", ctypes.c_char_p)]

    class WSAMSG(Structure):
        _pack_ = 8
        _fields_ = [
            ("name", ctypes.c_void_p),
            ("namelen", ctypes.c_int),
            ("lpBuffers", POINTER(WSABUF)),
            ("dwBufferCount", wintypes.ULONG),
            ("Control", WSABUF),
            ("dwFlags", wintypes.ULONG),
        ]

    ws2 = ctypes.WinDLL("ws2_32", use_last_error=True)
    SOCKET = wintypes.UINT_PTR
    WSASendMsg = ws2.WSASendMsg
    WSASendMsg.argtypes = [
        SOCKET,
        POINTER(WSAMSG),
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
        wintypes.LPVOID,
        wintypes.LPVOID,
    ]
    WSASendMsg.restype = ctypes.c_int
    WSAGetLastError = ws2.WSAGetLastError
    WSAGetLastError.argtypes = ()
    WSAGetLastError.restype = wintypes.DWORD
    class WSACMSGHDR(Structure):
        # Must match ws2def.h: SIZE_T cmsg_len; INT cmsg_level; INT cmsg_type;
        _pack_ = 8
        _fields_ = [
            ("cmsg_len", ctypes.c_size_t),
            ("cmsg_level", wintypes.INT),
            ("cmsg_type", wintypes.INT),
        ]

    _hs = ctypes.sizeof(WSACMSGHDR)
    _al = ctypes.sizeof(ctypes.c_size_t)

    def _wsa_align(n: int) -> int:
        return ((n + _al - 1) // _al) * _al

    return {
        "ctypes": ctypes,
        "byref": byref,
        "POINTER": POINTER,
        "WSABUF": WSABUF,
        "WSAMSG": WSAMSG,
        "WSACMSGHDR": WSACMSGHDR,
        "WSASendMsg": WSASendMsg,
        "WSAGetLastError": WSAGetLastError,
        "SOCKET_ERROR": -1,
        "wsa_cmsg_hdr_size": _hs,
        "wsa_cmsg_align": _wsa_align,
    }


def _win_wsasendmsg_ipv6_pktinfo(
    sock: socket.socket,
    wire: bytes,
    dest: tuple,
    src_addr: str,
    ifindex: int,
) -> bool:
    """Pin IPv6 source with IPV6_PKTINFO using WSASendMsg when stdlib sendmsg is missing."""
    global _win_wsasendmsg_cache
    if sys.platform != "win32":
        return False
    if _win_wsasendmsg_cache is False:
        return False
    if _win_wsasendmsg_cache is None:
        try:
            _win_wsasendmsg_cache = _win_wsasendmsg_load()
        except Exception:
            _win_wsasendmsg_cache = False
            return False
    st = _win_wsasendmsg_cache
    if st is False:
        return False

    ctypes = st["ctypes"]
    byref = st["byref"]
    POINTER = st["POINTER"]
    WSABUF = st["WSABUF"]
    WSAMSG = st["WSAMSG"]
    WSASendMsg = st["WSASendMsg"]
    WSAGetLastError = st["WSAGetLastError"]
    SOCKET_ERROR = st["SOCKET_ERROR"]
    WSACMSGHDR = st["WSACMSGHDR"]
    wsa_hdr_sz = int(st["wsa_cmsg_hdr_size"])
    wsa_align = st["wsa_cmsg_align"]

    dhost = dest[0]
    dport = int(dest[1])
    dflow = int(dest[2]) if len(dest) > 2 else 0
    dscope = int(dest[3]) if len(dest) > 3 else 0
    addr_b = socket.inet_pton(socket.AF_INET6, dhost)
    sa = struct.pack(
        "@HHI16sI",
        socket.AF_INET6,
        socket.htons(dport),
        dflow,
        addr_b,
        dscope,
    )
    sa_buf = ctypes.create_string_buffer(sa, len(sa))

    pkt_typ = _ipv6_pktinfo_cmsg_type()
    ipproto_v6 = int(socket.IPPROTO_IPV6)

    def _one_send(pktinfo_body: bytes) -> bool:
        dlen = len(pktinfo_body)
        cmsg_len_val = wsa_hdr_sz + dlen
        cmsg_space = wsa_align(wsa_hdr_sz) + wsa_align(dlen)
        raw_ctrl = bytearray(cmsg_space)
        ctrl = (ctypes.c_char * cmsg_space).from_buffer(raw_ctrl)
        h = WSACMSGHDR()
        h.cmsg_len = cmsg_len_val
        h.cmsg_level = ipproto_v6
        h.cmsg_type = pkt_typ
        ctypes.memmove(ctypes.addressof(ctrl), ctypes.byref(h), wsa_hdr_sz)
        raw_ctrl[wsa_hdr_sz : wsa_hdr_sz + dlen] = pktinfo_body

        data_buf = ctypes.create_string_buffer(wire)
        bufs = (WSABUF * 1)()
        bufs[0].len = len(wire)
        bufs[0].buf = ctypes.cast(ctypes.addressof(data_buf), ctypes.c_char_p)

        ctrl_wb = WSABUF()
        ctrl_wb.len = cmsg_space
        ctrl_wb.buf = ctypes.cast(ctypes.addressof(ctrl), ctypes.c_char_p)

        msg = WSAMSG()
        ctypes.memset(ctypes.byref(msg), 0, ctypes.sizeof(WSAMSG))
        msg.name = ctypes.cast(ctypes.addressof(sa_buf), ctypes.c_void_p)
        msg.namelen = len(sa)
        msg.lpBuffers = ctypes.cast(bufs, POINTER(WSABUF))
        msg.dwBufferCount = 1
        msg.Control = ctrl_wb
        msg.dwFlags = 0

        r = WSASendMsg(sock.fileno(), byref(msg), 0, None, None, None)
        if r == SOCKET_ERROR:
            if _raw_ptr_debug():
                err = int(WSAGetLastError())
                print(f"mDNS raw-ptr: WSASendMsg failed WSAError={err}", file=sys.stderr)
            return False
        return True

    pktinfo_body = _ipv6_in6_pktinfo_bytes(src_addr, ifindex)
    if _one_send(pktinfo_body):
        return True
    if ifindex != 0:
        return _one_send(_ipv6_in6_pktinfo_bytes(src_addr, 0))
    return False


def _win_try_sendmsg_ptr(
    sock: socket.socket,
    wire: bytes,
    dest: tuple,
    src_addr: str,
    ifindex: int,
) -> bool:
    """Return True if WSASendMsg or stdlib sendmsg(IPV6_PKTINFO) sent the wire."""
    # On Windows, prefer ctypes WSASendMsg first: stdlib sendmsg often fails or is
    # picky about ancillary layout; WSASendMsg matches our WSACMSGHDR packing.
    if sys.platform == "win32" and _win_wsasendmsg_ipv6_pktinfo(
        sock, wire, dest, src_addr, ifindex
    ):
        return True
    if _win_sendmsg_supported(sock):
        try:
            pktinfo = _ipv6_in6_pktinfo_bytes(src_addr, ifindex)
            pkt_typ = _ipv6_pktinfo_cmsg_type()
            sock.sendmsg(
                [wire],
                [(socket.IPPROTO_IPV6, pkt_typ, pktinfo)],
                0,
                dest,
            )
            return True
        except (OSError, AttributeError, TypeError) as e:
            if _raw_ptr_debug():
                print(f"mDNS raw-ptr: sendmsg(IPV6_PKTINFO) failed: {e!r}", file=sys.stderr)
    if sys.platform == "win32":
        return False
    return _win_wsasendmsg_ipv6_pktinfo(sock, wire, dest, src_addr, ifindex)


def _fmt_addrs(info: ServiceInfo) -> str:
    parts = []
    for a in info.parsed_addresses():
        parts.append(a)
    return ", ".join(parts) if parts else "(no addresses)"


def _fmt_props(info: ServiceInfo) -> str:
    if not info.properties:
        return ""
    items = []
    for k, v in info.properties.items():
        key = k.decode("utf-8", "replace") if isinstance(k, bytes) else k
        val = v.decode("utf-8", "replace") if isinstance(v, bytes) else v
        items.append(f"{key}={val}")
    return " | ".join(items)


class DectListener(ServiceListener):
    def __init__(self) -> None:
        self.seen: set[tuple[str, str]] = set()

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        key = (type_, name)
        if key in self.seen:
            return
        self.seen.add(key)
        info = zc.get_service_info(type_, name, timeout=8000)
        if info is None:
            print(f"[+] {type_:28} {name}\n    (get_service_info timeout)")
            return
        print(f"[+] {type_:28} {name}")
        print(f"    host={info.server} port={info.port} addresses={_fmt_addrs(info)}")
        txt = _fmt_props(info)
        if txt:
            print(f"    txt: {txt}")
        print()

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        print(f"[-] removed {type_} {name}")

    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        pass


def _zeroconf_empty_hint(zc_kwargs: dict) -> None:
    """When Wireshark shows mDNS but ServiceBrowser never calls add_service."""
    print(
        f"\nmDNS browse (Zeroconf; kwargs={zc_kwargs!r}): no services reported in the wait window.\n"
        "If Wireshark still shows answers to **ff02::fb** (UDP **5353**), python-zeroconf may "
        "have failed to **join** that multicast group (see **--zeroconf-debug**): on **Windows**, "
        "another stack often holds **UDP 5353** (Bonjour, “Function Discovery”, etc.), which "
        "returns **EADDRINUSE** / join failure while Wireshark still captures traffic. Check "
        "**`Get-NetUDPEndpoint -LocalPort 5353`** (PowerShell).\n"
        "Also allow inbound **UDP 5353** / ephemeral UDP for **python.exe** on **Private** networks.\n"
        "Manual fallback: **--raw-ptr** with **`--ipv6 \"fe80::…%<ifIndex>\"`** (see docstring).\n",
        file=sys.stderr,
    )


def _ip_version(s: str) -> IPVersion:
    s = s.strip().lower()
    if s in ("4", "v4", "ipv4"):
        return IPVersion.V4Only
    if s in ("6", "v6", "ipv6"):
        return IPVersion.V6Only
    return IPVersion.All


def _build_zeroconf_kwargs(
    ip_version: IPVersion,
    ifname: Optional[str],
    ipv6_bind: Optional[str],
) -> dict:
    kwargs: dict = {"ip_version": ip_version}

    if ifname is not None:
        try:
            kwargs["interfaces"] = [socket.if_nametoindex(ifname)]
        except OSError as e:
            raise OSError(f"cannot resolve interface {ifname!r}: {e}") from e
        return kwargs

    if ipv6_bind is not None:
        addr, scope = _parse_ipv6_bind(ipv6_bind)
        if scope == 0 and sys.platform == "win32":
            inferred = _windows_interface_index_for_bound_ipv6(addr)
            if inferred > 0:
                scope = inferred
        if scope > 0:
            # Zeroconf expects interface identifiers as IPv6 **addr%zone** strings (or
            # InterfaceChoice), not bare ifindex ints: on Windows, ``interfaces=[4]`` often
            # fails to join ff02::fb so ServiceBrowser never fires while Wireshark shows mDNS.
            kwargs["interfaces"] = [f"{addr}%{scope}"]
        elif addr:
            kwargs["interfaces"] = [addr]
        return kwargs

    return kwargs


_QTYPE_NAMES: dict[int, str] = {
    1: "A",
    2: "NS",
    5: "CNAME",
    12: "PTR",
    16: "TXT",
    28: "AAAA",
    33: "SRV",
    47: "NSEC",
}


def _decode_dns_name(data: bytes, off: int, *, _depth: int = 0) -> tuple[str, int]:
    """Decode a DNS domain name at ``off``; return (dotted name, offset after name)."""
    if _depth > 32:
        raise ValueError("DNS name: compression depth exceeded")
    labels: list[str] = []
    jumped = False
    resume: int | None = None
    pos = off
    while True:
        if pos >= len(data):
            raise ValueError("DNS name: truncated")
        length = data[pos]
        if length == 0:
            pos += 1
            end = resume if jumped else pos
            return (".".join(labels) + "." if labels else "."), end
        if (length & 0xC0) == 0xC0:
            if pos + 1 >= len(data):
                raise ValueError("DNS name: truncated pointer")
            ptr = ((length & 0x3F) << 8) | data[pos + 1]
            if resume is None:
                resume = pos + 2
            pos = ptr
            jumped = True
            continue
        if length > 63:
            raise ValueError(f"DNS name: bad label length {length}")
        pos += 1
        if pos + length > len(data):
            raise ValueError("DNS name: truncated label")
        labels.append(data[pos : pos + length].decode("utf-8", "replace"))
        pos += length


def _fmt_txt_rdata(rdata: bytes) -> str:
    if not rdata:
        return "(empty TXT)"
    parts: list[str] = []
    pos = 0
    while pos < len(rdata):
        chunk_len = rdata[pos]
        pos += 1
        if pos + chunk_len > len(rdata):
            return f"(bad TXT) hex={rdata.hex()}"
        chunk = rdata[pos : pos + chunk_len]
        pos += chunk_len
        try:
            parts.append(chunk.decode("utf-8", "replace"))
        except Exception:
            parts.append(repr(chunk))
    return " | ".join(parts) if parts else "(empty strings)"


def _fmt_rr_rdata(data: bytes, rtype: int, rdata: bytes, rdata_abs_off: int) -> str:
    if rtype == 12 and rdata_abs_off + len(rdata) <= len(data):
        try:
            target, _ = _decode_dns_name(data, rdata_abs_off)
            return f"-> {target}"
        except ValueError as e:
            return f"(PTR name error: {e})"
    if rtype == 16:
        return _fmt_txt_rdata(rdata)
    if rtype == 28 and len(rdata) >= 16:
        return socket.inet_ntop(socket.AF_INET6, rdata[:16])
    if rtype == 33 and len(rdata) >= 6:
        prio, weight, port = struct.unpack("!HHH", rdata[:6])
        try:
            target, _ = _decode_dns_name(data, rdata_abs_off + 6)
            return f"prio={prio} weight={weight} port={port} target={target}"
        except ValueError as e:
            return f"prio={prio} weight={weight} port={port} (target err: {e})"
    if not rdata:
        return "(no RDATA)"
    hx = rdata.hex()
    return (hx[:96] + "…") if len(hx) > 96 else hx


def _summarize_mdns_wire(data: bytes) -> str:
    """
    Walk DNS sections without dnslib (handles mDNS empty TXT: RDLENGTH 0).
    """
    if len(data) < 12:
        return f"(too short: {len(data)} octets)"
    qd, an, ns, ar = struct.unpack("!HHHH", data[4:12])
    tid, flags = struct.unpack("!HH", data[0:4])
    lines = [
        f"loose decode: id=0x{tid:04x} flags=0x{flags:04x} "
        f"QD={qd} AN={an} NS={ns} AR={ar}"
    ]
    off = 12

    def skip_question(o: int) -> int:
        _, o = _decode_dns_name(data, o)
        return o + 4

    def one_rr(o: int, section: str) -> tuple[str, int]:
        name, o = _decode_dns_name(data, o)
        if o + 10 > len(data):
            raise ValueError("truncated RR header")
        rtype, rclass, ttl, rdlen = struct.unpack("!HHIH", data[o : o + 10])
        o += 10
        if o + rdlen > len(data):
            raise ValueError("truncated RDATA")
        rdata_abs = o
        rdata = data[o : o + rdlen]
        o += rdlen
        tname = _QTYPE_NAMES.get(rtype, str(rtype))
        detail = _fmt_rr_rdata(data, rtype, rdata, rdata_abs)
        block = f"  [{section}] {name} {tname} class=0x{rclass:04x} ttl={ttl}\n      {detail}"
        return block, o

    try:
        for _ in range(qd):
            lines.append("  [QD] (question)")
            off = skip_question(off)
        for _ in range(an):
            line, off = one_rr(off, "AN")
            lines.append(line)
        for _ in range(ns):
            line, off = one_rr(off, "NS")
            lines.append(line)
        for _ in range(ar):
            line, off = one_rr(off, "AR")
            lines.append(line)
        if off != len(data):
            lines.append(f"  (warning: {len(data) - off} trailing octets)")
    except ValueError as e:
        lines.append(f"  (walk stopped: {e} at offset {off})")
    return "\n".join(lines)


def _raw_ptr_zero_recv_hint(bound: tuple, addr: str, scope: int, plat: str) -> None:
    """Explain common Windows vs-Wireshark gaps when recv gets no datagrams."""
    port = bound[1] if len(bound) > 1 else 0
    kind = _ipv6_scope_kind(addr)
    print("", file=sys.stderr)
    print(
        "mDNS raw-ptr: received 0 UDP datagrams while Wireshark shows traffic — typical causes:",
        file=sys.stderr,
    )
    print(
        f"  • Replies are unicast to a **different** IPv6/port than this socket accepts. "
        f"Compare Wireshark **query** IPv6 Source to ``--ipv6``, and reply UDP **Destination** "
        f"port to {port}. On Windows, **IPV6_PKTINFO** (stdlib ``sendmsg`` or ``WSASendMsg``) "
        f"pins the query source; if pinned send fails, ``sendto`` may pick a **temporary** GUA "
        f"and recv stays empty.",
        file=sys.stderr,
    )
    if kind == "gua" and plat == "win32":
        print(
            "  • **GUA:** In Wireshark, if the **outgoing** query’s IPv6 Source ≠ your ``--ipv6`` "
            "address, bind to the address the OS actually used, or use a **non-temporary** / "
            "stable GUA. Ensure you run the latest script (WSASendMsg fallback when ``sendmsg`` "
            "is missing).",
            file=sys.stderr,
        )
        print(
            "  • **GUA + temp:** If **Ethernet dst** in the capture is **not** your PC’s MAC "
            "(e.g. frame shows identical src/dst MAC), the capture is not on the tether "
            "adapter — the script only sees what reaches that NIC.",
            file=sys.stderr,
        )
    if kind == "ula":
        print(
            "  • **ULA:** If Wireshark on the **tether** NIC shows **no** mDNS **responses** at "
            "all, devices may not be receiving the query (enable bridge **LL multicast** tap + "
            "mDNS NAT in firmware, correct ``%`` interface). If **responses** appear in Wireshark "
            "but this script still prints 0, the issue is the same **unicast recv** mismatch as "
            "for GUA (query source vs bound address / sendmsg).",
            file=sys.stderr,
        )
    print(
        "  • **Windows Defender Firewall** (or other): inbound UDP to this socket blocked "
        "for python.exe — Wireshark still captures on the adapter. Allow Private-network "
        "inbound UDP or add a rule for your Python path.",
        file=sys.stderr,
    )
    if plat == "win32":
        print(
            "  • Confirm the adapter is **Private** (Settings → Network) so the firewall "
            "profile matches your rule.",
            file=sys.stderr,
        )
    z = scope if scope > 0 else (bound[3] if len(bound) > 3 and bound[3] else 0)
    if z > 0:
        print(
            f"  • Tether / coexistence: try link-local on this interface, e.g. "
            f"fe80::…%{z} (see Get-NetIPAddress).",
            file=sys.stderr,
        )
    else:
        print(
            "  • Tether / coexistence: try link-local with zone index "
            "fe80::…%<ifIndex> on the shield adapter.",
            file=sys.stderr,
        )


def _run_raw_ptr(
    ipv6_bind: str,
    timeout: float,
    qname: str,
) -> int:
    """
    Send PTR for ``qname`` to ff02::fb:5353 and print any UDP/5353 replies.
    Joins multicast on ``ipv6_bind``'s scope (fixes missing replies with Zeroconf on Windows).

    Uses **dnslib** for queries and for replies when possible. mDNS may send **TXT
    with RDLENGTH 0**; dnslib raises ``Empty RR`` for that — a loose decode is printed
    instead (same CLASS/TTL handling as dnspython issues are avoided for that path).
    """
    try:
        from dnslib import DNSRecord
    except ImportError:
        print("Raw mode needs dnslib:  py -m pip install dnslib", file=sys.stderr)
        return 1

    addr, scope = _parse_ipv6_bind(ipv6_bind)
    if scope == 0:
        inferred = _windows_interface_index_for_bound_ipv6(addr)
        if inferred > 0:
            scope = inferred
            if _raw_ptr_debug():
                print(
                    f"mDNS raw-ptr: Windows interface index {scope} for multicast join "
                    f"(auto from address {addr}; or pass --ipv6 '{addr}%{scope}')",
                    file=sys.stderr,
                )
    if scope == 0 and addr.lower().startswith("fe80:"):
        print(
            "For link-local bind, use zone index:  --ipv6 'fe80::…%12'  (12 = interface index)",
            file=sys.stderr,
        )
    elif scope == 0 and sys.platform == "win32":
        print(
            "mDNS raw-ptr: sin6_scope_id is 0. Unicast replies may not reach this socket "
            "(Wireshark can still show them). Use --ipv6 'ADDR%<InterfaceIndex>' "
            "(see Get-NetIPAddress / Get-NetIPInterface).",
            file=sys.stderr,
        )

    qn = qname.rstrip(".") + "."
    # dnslib's question() uses getattr(QTYPE, qtype): second arg must be the type *name* (str).
    query = DNSRecord.question(qn, "PTR")
    wire = query.pack()

    sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    try:
        sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
    except OSError:
        pass
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    except OSError:
        pass

    bind_scope = 0
    last_bind_err: OSError | None = None
    use_win_wildcard_recv = False

    if sys.platform == "win32" and scope > 0 and not _ipv6_is_link_local(addr):
        last_bind_err = None
        # Prefer :: + interface index: unicast mDNS replies target the query's IPv6
        # (ULA/GUA) but Winsock often will not deliver them to a socket bound only to
        # (addr, port, 0, 0) — Wireshark still shows the datagrams on the tether NIC.
        try:
            sock.bind(("::", 0, 0, scope))
            bind_scope = scope
            use_win_wildcard_recv = True
            if _raw_ptr_debug():
                print(
                    f"mDNS raw-ptr: Windows bind :: sin6_scope_id={scope} "
                    "(recv unicast mDNS replies for any IPv6 on this interface).",
                    flush=True,
                )
        except OSError as e_wild:
            last_bind_err = e_wild
            narrow_ok = False
            # Some Windows drivers accept a zone suffix on the host string but reject
            # bind(("::", …, ifindex)); that form often demuxes unicast mDNS better than
            # (addr,0,0,0) alone for temporary GUAs on the tether NIC.
            if scope > 0:
                try:
                    sock.bind((f"{addr}%{scope}", 0))
                    bind_scope = scope
                    last_bind_err = None
                    narrow_ok = True
                    use_win_wildcard_recv = False
                    if _raw_ptr_debug():
                        print(
                            f"mDNS raw-ptr: Windows scoped literal bind {addr!r}%{scope} "
                            f"(after :: bind failed: {e_wild})",
                            file=sys.stderr,
                            flush=True,
                        )
                except OSError as e_pct:
                    if _raw_ptr_debug():
                        print(
                            f"mDNS raw-ptr: scoped literal bind failed: {e_pct!r}",
                            file=sys.stderr,
                            flush=True,
                        )
            if not narrow_ok:
                for cand in (0, scope):
                    bt: tuple = (addr, 0, 0, cand)
                    try:
                        sock.bind(bt)
                        bind_scope = cand
                        last_bind_err = None
                        use_win_wildcard_recv = False
                        break
                    except OSError as e:
                        last_bind_err = e
                        continue
            if last_bind_err is not None:
                print(
                    f"mDNS raw-ptr: bind :: failed ({e_wild}); "
                    f"bind {addr!r} with sin6_scope_id 0 and {scope} failed ({last_bind_err})",
                    file=sys.stderr,
                )
            elif not narrow_ok and not use_win_wildcard_recv:
                print(
                    f"mDNS raw-ptr: note: bind('::', ifindex={scope}) failed ({e_wild!r}); "
                    "using (addr,0,0,0) — if Done(0) but Wireshark shows unicast replies to "
                    "this IPv6+port, allow inbound UDP for python.exe (Private profile) or "
                    "try --ipv6 fe80::…%<same ifindex>.",
                    file=sys.stderr,
                )
    else:
        bind_scope_candidates = [_windows_ipv6_bind_scope_id(addr, scope)]
        for cand in bind_scope_candidates:
            bt = (addr, 0, 0, cand)
            try:
                sock.bind(bt)
                bind_scope = cand
                last_bind_err = None
                break
            except OSError as e:
                last_bind_err = e
                continue

    if last_bind_err is not None:
        print(f"bind failed: {last_bind_err}", file=sys.stderr)
        sock.close()
        return 1

    bound = sock.getsockname()

    mcast = socket.inet_pton(socket.AF_INET6, "ff02::fb")
    mreq = mcast + struct.pack("@I", scope if scope > 0 else 0)
    try:
        sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_JOIN_GROUP, mreq)
    except OSError as e:
        print(f"IPV6_JOIN_GROUP ff02::fb failed (try correct %zone): {e}", file=sys.stderr)
        sock.close()
        return 1

    try:
        if scope > 0:
            mif = getattr(socket, "IPV6_MULTICAST_IF", None)
            if mif is not None:
                sock.setsockopt(socket.IPPROTO_IPV6, mif, struct.pack("@I", scope))
    except OSError:
        pass

    # RFC 6762: mDNS packets MUST use TTL 255 (IPv4) / Hop Limit 255 (IPv6). Many
    # responders ignore queries with a lower hop limit; Windows defaults multicast
    # hops to 1 unless IPV6_MULTICAST_HOPS is set.
    _mhops = getattr(socket, "IPV6_MULTICAST_HOPS", None)
    if _mhops is not None:
        try:
            sock.setsockopt(socket.IPPROTO_IPV6, _mhops, 255)
        except OSError:
            pass

    dest = ("ff02::fb", 5353, 0, scope if scope > 0 else 0)
    win_ula_gua = sys.platform == "win32" and scope > 0 and not _ipv6_is_link_local(addr)

    def _ptr_send(*, quiet: bool) -> None:
        if use_win_wildcard_recv:
            if _win_try_sendmsg_ptr(sock, wire, dest, addr, scope):
                if not quiet:
                    print(
                        f"Sent PTR query {qn!r} -> {dest} (IPV6_PKTINFO src={addr!r}, "
                        f"ephemeral port {bound[1]})"
                    )
            else:
                if _raw_ptr_debug() and not quiet:
                    print(
                        "mDNS raw-ptr: pinned send (sendmsg/WSASendMsg) failed; using sendto — "
                        "Wireshark query **IPv6 Source** may not match --ipv6.",
                        file=sys.stderr,
                    )
                sock.sendto(wire, dest)
                if not quiet:
                    print(f"Sent PTR query {qn!r} -> {dest} (query from {bound!r})")
            return
        if win_ula_gua and _win_try_sendmsg_ptr(sock, wire, dest, addr, scope):
            if not quiet:
                print(
                    f"Sent PTR query {qn!r} -> {dest} (IPV6_PKTINFO src={addr!r}, "
                    f"ephemeral port {bound[1]})"
                )
            return
        sock.sendto(wire, dest)
        if not quiet:
            print(f"Sent PTR query {qn!r} -> {dest} (query from {bound!r})")

    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1 << 20)
    except OSError:
        pass

    t0 = time.monotonic()
    deadline = t0 + max(0.5, timeout)
    second_scoped_nl_probe_at = (
        (t0 + 0.25) if (scope > 0 and not _ipv6_is_link_local(addr)) else None
    )
    second_scoped_nl_sent = False
    initial_sent = False

    print(
        f"Receive window {timeout:.1f}s on UDP port {bound[1]} "
        f"(mDNS replies typically src port 5353)…\n",
        flush=True,
    )

    n = 0
    try:
        sock.settimeout(None)
    except OSError:
        pass

    while True:
        now = time.monotonic()
        if now >= deadline:
            break

        if not initial_sent:
            try:
                pre_r, _, _ = select.select([sock], [], [], 0.0)
            except (OSError, ValueError):
                pre_r = []
            if pre_r:
                try:
                    sock.recvfrom(65535)
                except OSError:
                    pass
            _ptr_send(quiet=False)
            initial_sent = True

        if (
            initial_sent
            and second_scoped_nl_probe_at is not None
            and not second_scoped_nl_sent
            and now >= second_scoped_nl_probe_at
        ):
            _ptr_send(quiet=True)
            second_scoped_nl_sent = True

        remain = deadline - now
        if remain <= 0.0:
            break

        to_wait = min(remain, 1.0)
        if second_scoped_nl_probe_at is not None and not second_scoped_nl_sent:
            probe_in = second_scoped_nl_probe_at - now
            if probe_in > 0.0:
                to_wait = min(to_wait, probe_in)
        to_wait = max(to_wait, 1e-6)

        try:
            readable, _, _ = select.select([sock], [], [], to_wait)
        except (OSError, ValueError):
            readable = None

        batch: list[tuple[bytes, tuple]] = []

        if readable is None:
            try:
                sock.settimeout(to_wait)
                try:
                    batch.append(sock.recvfrom(65535))
                except socket.timeout:
                    continue
            except OSError as e:
                print(f"recv: {e}", file=sys.stderr)
                break
            finally:
                try:
                    sock.settimeout(None)
                except OSError:
                    pass
        elif not readable:
            continue
        else:
            try:
                batch.append(sock.recvfrom(65535))
            except OSError as e:
                print(f"recv: {e}", file=sys.stderr)
                break

        try:
            sock.settimeout(0.0)
            while True:
                try:
                    batch.append(sock.recvfrom(65535))
                except socket.timeout:
                    break
        except OSError:
            pass
        finally:
            try:
                sock.settimeout(None)
            except OSError:
                pass

        for data, src in batch:
            n += 1
            print(f"--- reply #{n} from {src} ({len(data)} octets) ---")
            try:
                reply = DNSRecord.parse(data)
                print(str(reply))
            except Exception as e:
                # dnslib rejects mDNS-empty TXT (RDLENGTH 0); that is normal, not a failure.
                err = str(e).lower()
                if "empty rr" not in err:
                    print(f"(dnslib parse failed: {e}; loose decode below)\n")
                try:
                    print(_summarize_mdns_wire(data))
                except Exception as e2:
                    print(f"(loose decode failed: {e2})\nhex[:200]={data[:200].hex()}")
            print()

    sock.close()
    print(f"Done ({n} datagram(s)).")
    if n == 0:
        port = int(bound[1]) if len(bound) > 1 else 0
        if sys.platform == "win32":
            print(
                "\nmDNS raw-ptr: received no UDP on this socket. If Wireshark shows a **unicast** "
                f"reply to **{addr}** with **UDP destination port {port}**, Windows often "
                "**blocks inbound UDP to python.exe** (Private profile) even though the NIC "
                "sees the packet — add a firewall allow rule or try "
                "`--ipv6 \"fe80::…%<InterfaceIndex>\"` (link-local). "
                "If the capture shows **multiple AAAA** answers (LL + ULA + GUA), the device "
                "side is fine; the gap is still delivery to this process/socket. "
                f"Wireshark filter example: `ipv6.addr == {addr} && udp.port == {port}`. "
                "Set **MDNS_RAW_PTR_DEBUG=1** for bind/send/recv diagnostics.\n",
                file=sys.stderr,
            )
        else:
            print(
                "\nmDNS raw-ptr: 0 datagrams. If tcpdump shows replies to this address+UDP port, "
                "check local firewall and that capture is on the same interface as --ipv6 / %zone. "
                "Set **MDNS_RAW_PTR_DEBUG=1** for more hints.\n",
                file=sys.stderr,
            )
        if _raw_ptr_debug():
            _raw_ptr_zero_recv_hint(bound, addr, scope, sys.platform)
    return 0


def main() -> int:
    p = argparse.ArgumentParser(
        description="mDNS browse for _dect-nr._udp (IPv6 default; pin interface)."
    )
    p.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Seconds to run browse (default: 10)",
    )
    p.add_argument(
        "--ip-version",
        choices=("6", "4", "all"),
        default="6",
        help="mDNS stack: 6 = IPv6-only (default; use for DECT). 4 = IPv4-only. all = dual.",
    )
    g = p.add_mutually_exclusive_group()
    g.add_argument(
        "--ifname",
        metavar="NAME",
        help="Outgoing mDNS only on this interface (e.g. eth0, enX0). Uses if_nametoindex().",
    )
    g.add_argument(
        "--ipv6",
        metavar="ADDR",
        help="Bind mDNS to this IPv6 address. On Windows use …%%<InterfaceIndex> "
        "(link-local, global, ULA) so unicast replies are received; script may infer %% "
        "via Get-NetIPAddress when omitted.",
    )
    p.add_argument(
        "--types",
        nargs="*",
        default=["_dect-nr._udp.local."],
        help="Service types to browse (default: _dect-nr._udp)",
    )
    p.add_argument(
        "--raw-ptr",
        action="store_true",
        help="Do not use Zeroconf; send PTR with dnslib + join ff02::fb (see docstring).",
    )
    p.add_argument(
        "--raw-qname",
        metavar="NAME",
        default="_dect-nr._udp.local",
        help="With --raw-ptr: PTR query name (default: _dect-nr._udp.local)",
    )
    p.add_argument(
        "--zeroconf-debug",
        action="store_true",
        help="Enable DEBUG logging for the zeroconf package.",
    )
    p.add_argument(
        "--no-raw-ptr-fallback",
        action="store_true",
        help="On Windows, do not run a short --raw-ptr probe if Zeroconf finds nothing "
        "(default: auto-fallback when --ipv6 is set).",
    )
    args = p.parse_args()

    if args.zeroconf_debug:
        logging.basicConfig(level=logging.DEBUG, format="%(levelname)s %(name)s: %(message)s")
        logging.getLogger("zeroconf").setLevel(logging.DEBUG)

    if args.raw_ptr:
        if not args.ipv6:
            print(
                "--raw-ptr requires --ipv6 (e.g. fe80::…%4 or 2001:…%4 on Windows for recv).",
                file=sys.stderr,
            )
            return 2
        return _run_raw_ptr(args.ipv6, args.timeout, args.raw_qname)

    ip_ver = _ip_version(args.ip_version)

    try:
        zc_kwargs = _build_zeroconf_kwargs(ip_ver, args.ifname, args.ipv6)
    except OSError as e:
        print(e, file=sys.stderr)
        return 1
    except ValueError as e:
        print(e, file=sys.stderr)
        return 1

    print("mDNS browse:", ", ".join(args.types))
    print("IP version:", args.ip_version, "| interface:", args.ifname or args.ipv6 or "(all interfaces)")
    print("Zeroconf kwargs:", zc_kwargs)
    print("Duration:", args.timeout, "s\n")

    listener = DectListener()
    zc: Optional[Zeroconf] = None
    browsers: list[ServiceBrowser] = []
    zc_empty = False

    try:
        zc = Zeroconf(**zc_kwargs)
        for st in args.types:
            if not st.endswith("."):
                st = st + "."
            browsers.append(ServiceBrowser(zc, st, listener))
        time.sleep(max(0.5, args.timeout))
        zc_empty = not listener.seen
        if zc_empty:
            _zeroconf_empty_hint(zc_kwargs)
    except OSError as e:
        print("Socket / bind error (bad --ifname/--ipv6 or firewall?):", e, file=sys.stderr)
        return 1
    finally:
        for b in browsers:
            b.cancel()
        if zc is not None:
            zc.close()

    print("Done (Zeroconf browse).")
    if (
        zc_empty
        and sys.platform == "win32"
        and args.ipv6
        and not args.no_raw_ptr_fallback
    ):
        raw_qname = args.types[0].rstrip(".")
        fb_t = min(12.0, max(5.0, args.timeout * 0.3))
        print(
            f"\nZeroconf found nothing; running **--raw-ptr** fallback ({fb_t:.0f}s) for "
            f"{raw_qname!r} (first of --types). Use **--no-raw-ptr-fallback** to skip.\n",
            file=sys.stderr,
        )
        return _run_raw_ptr(args.ipv6, fb_t, raw_qname)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
