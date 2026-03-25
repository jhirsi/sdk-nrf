/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * mDNS browse for DNS-SD services advertised on the mesh, then AAAA lookup per
 * host (resolver invokes the callback once per AAAA RR). Long RD ID is taken from
 * the IPv6 address via dect_utils (same mapping
 * as elsewhere in the DECT stack). When CONFIG_NET_L2_DECT_ULA has installed an
 * on-link /96 on dect0, prints a derived on-link ULA for each peer (same layout
 * as dect_net_l2_ipv6_ula_sync_for_peer) so link-local mDNS answers map to a
 * reachable ULA.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/shell/shell.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/hostname.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/sys/byteorder.h>

#include <net/dect/dect_net_l2.h>
#include <net/dect/dect_utils.h>

#include "desh_print.h"

#define DECT_DISCOVER_MAX_HOSTS 24

/*
 * One data row <= DECT_DISCOVER_ROW_MAX:
 *  sp + %2u + " | " + host(W) + " | " + kind(4) + " | " + ipv6(W) + " | " + ula(W) + " | " + ...
 */
#define DECT_DISCOVER_ROW_MAX 140
#define DECT_DISCOVER_HOST_W  24
#define DECT_DISCOVER_KIND_W  4
#define DECT_DISCOVER_IPV6_W  31

/** On-DECT ULA /96: first 64 bits from CONFIG_NET_L2_DECT_ULA_PREFIX, then 32-bit RD id. */
#define DECT_DISCOVER_ULA_PFX64_BYTES 8
#define DECT_DISCOVER_ULA_PFX96_BYTES 12

#define SUFFIX_DECT_NR	"._dect-nr._udp.local"

/*
 * PTR browse: keep resolver budget and shell-side slack modest so a quiet mesh does
 * not block ~16s per attempt (CONFIG_NET_SOCKETS_DNS_TIMEOUT + large slack).
 * AAAA uses full CONFIG_NET_SOCKETS_DNS_TIMEOUT + larger slack.
 */
#define DISCOVER_PTR_DNS_BUDGET_MS 5000
#define DISCOVER_PTR_SLACK_MS	    2500U
#define DISCOVER_AAAA_SLACK_MS	    6000U
#define DISCOVER_WAIT_TICK_MS	    2000U

/* mDNS .local queries use dns_id 0; cancel must match query name+type when possible. */
static int discover_wait_done(struct dns_resolve_context *ctx, uint16_t dns_id, struct k_sem *sem,
			      const char *qname, enum dns_query_type qtype,
			      int32_t dns_budget_ms, uint32_t slack_ms)
{
	uint32_t wait_total;
	uint32_t remaining;

	if (dns_budget_ms < 1000) {
		dns_budget_ms = CONFIG_NET_SOCKETS_DNS_TIMEOUT;
	}

	wait_total = (uint32_t)dns_budget_ms + slack_ms;
	remaining = wait_total;

	while (remaining > 0U) {
		uint32_t step = remaining > DISCOVER_WAIT_TICK_MS ?
			DISCOVER_WAIT_TICK_MS : remaining;

		if (k_sem_take(sem, K_MSEC(step)) == 0) {
			return 0;
		}
		remaining -= step;
	}

	desh_warn("discover: query completion timeout (%u ms), cancel id %u", wait_total,
		  (unsigned int)dns_id);
	if (qname != NULL) {
		(void)dns_resolve_cancel_with_name(ctx, dns_id, qname, qtype);
	} else {
		(void)dns_resolve_cancel(ctx, dns_id);
	}
	(void)k_sem_take(sem, K_MSEC(1000));
	return -ETIMEDOUT;
}

static void discover_print_sep_row(void)
{
	/* Leading space + dashes (>= DECT_DISCOVER_ROW_MAX). */
	desh_print(" ----------------------------------------------------------------"
		   "----------------------------------------------------------------");
}

struct discover_ctx {
	struct k_sem wait;
	char hosts[DECT_DISCOVER_MAX_HOSTS][CONFIG_DNS_RESOLVER_MAX_NAME_LEN + 1];
	size_t n_hosts;
	char cur_host[CONFIG_DNS_RESOLVER_MAX_NAME_LEN + 1];
	size_t row_index; /* 1-based, matches resolve order */
	bool got_aaaa;
	struct net_if *dect0;
};

static void discover_ula_reach_to_str(struct net_if *dect0, const struct net_in6_addr *aaaa,
				      char *buf, size_t buflen)
{
	struct net_in6_addr pfx96;
	uint8_t plen_bits;
	struct net_in6_addr ula;
	uint32_t rd;

	if (buflen == 0U) {
		return;
	}

	if (dect0 == NULL ||
	    !dect_net_l2_ipv6_dect_ula_onlink_prefix_get(dect0, &pfx96, &plen_bits)) {
		(void)snprintf(buf, buflen, "-");
		return;
	}

	if (net_ipv6_is_ula_addr(aaaa)) {
		(void)net_addr_ntop(NET_AF_INET6, aaaa, buf, buflen);
		return;
	}

	if (!net_ipv6_is_ll_addr(aaaa)) {
		(void)snprintf(buf, buflen, "-");
		return;
	}

	rd = dect_utils_lib_long_rd_id_from_ipv6_addr((struct in6_addr *)&aaaa->s6_addr);

	memcpy(ula.s6_addr, pfx96.s6_addr, DECT_DISCOVER_ULA_PFX64_BYTES);
	sys_put_be32(rd, ula.s6_addr + DECT_DISCOVER_ULA_PFX64_BYTES);
	memcpy(ula.s6_addr + DECT_DISCOVER_ULA_PFX96_BYTES,
	       aaaa->s6_addr + DECT_DISCOVER_ULA_PFX96_BYTES,
	       NET_IPV6_ADDR_SIZE - DECT_DISCOVER_ULA_PFX96_BYTES);

	(void)net_addr_ntop(NET_AF_INET6, &ula, buf, buflen);
}

static const char *discover_ipv6_kind(const struct net_in6_addr *a)
{
	if (net_ipv6_is_ll_addr(a)) {
		return "LL";
	}
	if (net_ipv6_is_ula_addr(a)) {
		return "ULA";
	}
	if (net_ipv6_is_global_addr(a)) {
		return "GUA";
	}
	return "?";
}

static void str_ascii_tolower_inplace(char *s)
{
	for (; *s != '\0'; s++) {
		if (*s >= 'A' && *s <= 'Z') {
			*s = (char)(*s - 'A' + 'a');
		}
	}
}

static bool dns_ascii_eq_ci(const char *a, const char *b)
{
	for (; *a && *b; a++, b++) {
		char x = *a;
		char y = *b;

		if (x >= 'A' && x <= 'Z') {
			x += 32;
		}
		if (y >= 'A' && y <= 'Z') {
			y += 32;
		}
		if (x != y) {
			return false;
		}
	}

	return *a == *b;
}

static bool name_ends_with_ci(const char *name, const char *suf)
{
	size_t ln = strlen(name);
	size_t ls = strlen(suf);

	if (ls > ln) {
		return false;
	}

	return dns_ascii_eq_ci(name + ln - ls, suf);
}

static int host_add(struct discover_ctx *d, const char *hostlocal)
{
	if (d->n_hosts >= DECT_DISCOVER_MAX_HOSTS) {
		return -ENOMEM;
	}

	for (size_t i = 0; i < d->n_hosts; i++) {
		if (strcmp(d->hosts[i], hostlocal) == 0) {
			return 0;
		}
	}

	strncpy(d->hosts[d->n_hosts], hostlocal, CONFIG_DNS_RESOLVER_MAX_NAME_LEN);
	d->hosts[d->n_hosts][CONFIG_DNS_RESOLVER_MAX_NAME_LEN] = '\0';
	d->n_hosts++;

	return 0;
}

static int ptr_instance_to_hostlocal(const char *canon, char *out, size_t outlen)
{
	const char *suf = NULL;

	if (name_ends_with_ci(canon, SUFFIX_DECT_NR)) {
		suf = SUFFIX_DECT_NR;
	} else {
		return -ENOENT;
	}

	size_t clen = strlen(canon);
	size_t slen = strlen(suf);

	if (clen <= slen) {
		return -EINVAL;
	}

	size_t label_len = clen - slen;

	if (label_len + sizeof(".local") > outlen) {
		return -E2BIG;
	}

	memcpy(out, canon, label_len);
	memcpy(out + label_len, ".local", sizeof(".local"));
	str_ascii_tolower_inplace(out);

	return 0;
}

/* Resolver lowercases the on-wire QNAME for hashes; keep stored names lower too so
 * dns_resolve_cancel_with_name() matches the slot after AAAA / PTR waits.
 */
static bool discover_host_is_self_local(const char *hostlocal)
{
	char expect[CONFIG_DNS_RESOLVER_MAX_NAME_LEN + 1];
	int n;

	n = snprintf(expect, sizeof(expect), "%s.local", net_hostname_get());
	if (n <= 0 || (size_t)n >= sizeof(expect)) {
		return false;
	}

	return dns_ascii_eq_ci(hostlocal, expect);
}

static void ptr_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data)
{
	struct discover_ctx *d = user_data;

	if (status == DNS_EAI_INPROGRESS) {
		if (info != NULL && info->ai_family == NET_AF_LOCAL) {
			char hl[CONFIG_DNS_RESOLVER_MAX_NAME_LEN + 1];

			if (ptr_instance_to_hostlocal(info->ai_canonname, hl, sizeof(hl)) == 0) {
				if (host_add(d, hl) == -ENOMEM) {
					desh_warn("discover: host list full (%d)",
						  DECT_DISCOVER_MAX_HOSTS);
				}
			}
		}
		return;
	}

	/* Resolver also ends with DNS_EAI_FAIL / DNS_EAI_NODATA etc. (see resolve.c
	 * dispatcher_cb quit path); only ALLDONE/CANCELED would leave us stuck.
	 */
	k_sem_give(&d->wait);
}

static int32_t discover_ptr_dns_budget_ms(void)
{
	int32_t b = CONFIG_NET_SOCKETS_DNS_TIMEOUT;

	if (b > (int32_t)DISCOVER_PTR_DNS_BUDGET_MS) {
		b = (int32_t)DISCOVER_PTR_DNS_BUDGET_MS;
	}

	return b;
}

static int discover_run_ptr(struct dns_resolve_context *ctx, struct discover_ctx *d,
			    const char *service)
{
	uint16_t dns_id;
	int32_t ptr_budget = discover_ptr_dns_budget_ms();
	int ret;

	k_sem_init(&d->wait, 0, 1);
	ret = dns_resolve_service(ctx, service, &dns_id, ptr_cb, d, ptr_budget);
	if (ret == -EAGAIN) {
		desh_warn("discover: resolver busy (-EAGAIN), short delay then PTR %s", service);
		k_sleep(K_MSEC(250));
		k_sem_init(&d->wait, 0, 1);
		ret = dns_resolve_service(ctx, service, &dns_id, ptr_cb, d, ptr_budget);
	}
	if (ret < 0) {
		return ret;
	}

	ret = discover_wait_done(ctx, dns_id, &d->wait, service, DNS_QUERY_TYPE_PTR, ptr_budget,
				 DISCOVER_PTR_SLACK_MS);
	if (ret < 0) {
		desh_warn("discover: PTR %s timed out, retry once after backoff", service);
		k_sleep(K_MSEC(400 + (sys_rand32_get() % 400U)));
		k_sem_init(&d->wait, 0, 1);
		ret = dns_resolve_service(ctx, service, &dns_id, ptr_cb, d, ptr_budget);
		if (ret == -EAGAIN) {
			k_sleep(K_MSEC(250));
			ret = dns_resolve_service(ctx, service, &dns_id, ptr_cb, d, ptr_budget);
		}
		if (ret < 0) {
			return ret;
		}
		ret = discover_wait_done(ctx, dns_id, &d->wait, service, DNS_QUERY_TYPE_PTR,
					 ptr_budget, DISCOVER_PTR_SLACK_MS);
	}

	return ret;
}

static void aaaa_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data)
{
	struct discover_ctx *d = user_data;

	if (status == DNS_EAI_INPROGRESS) {
		if (info != NULL && info->ai_family == NET_AF_INET6) {
			struct in6_addr *addr = &net_sin6(&info->ai_addr)->sin6_addr;
			char ip[NET_IPV6_ADDR_LEN];
			char ula_str[NET_IPV6_ADDR_LEN];
			uint32_t rd = dect_utils_lib_long_rd_id_from_ipv6_addr(addr);

			d->got_aaaa = true;
			net_addr_ntop(NET_AF_INET6, addr, ip, sizeof(ip));
			discover_ula_reach_to_str(d->dect0, (const struct net_in6_addr *)addr,
						  ula_str,
						  sizeof(ula_str));
			desh_print(" %2u | %-*.*s | %-*.*s | %-*.*s | %-*.*s | %u (0x%08x)",
				   (unsigned int)d->row_index, DECT_DISCOVER_HOST_W,
				   DECT_DISCOVER_HOST_W, d->cur_host, DECT_DISCOVER_KIND_W,
				   DECT_DISCOVER_KIND_W,
				   discover_ipv6_kind((const struct net_in6_addr *)addr),
				   DECT_DISCOVER_IPV6_W, DECT_DISCOVER_IPV6_W, ip,
				   DECT_DISCOVER_IPV6_W, DECT_DISCOVER_IPV6_W, ula_str,
				   (unsigned int)rd, (unsigned int)rd);
		}
		return;
	}

	if (status == DNS_EAI_ALLDONE) {
		if (!d->got_aaaa) {
			desh_warn("discover: no AAAA for %.60s", d->cur_host);
		}
	} else if (status == DNS_EAI_CANCELED) {
		if (!d->got_aaaa) {
			desh_warn("discover: timeout for %.60s", d->cur_host);
		}
	} else if (!d->got_aaaa) {
		desh_warn("discover: AAAA for %.60s ended (%d)", d->cur_host, status);
	}

	k_sem_give(&d->wait);
}

static void dect_shell_discover_cmd(const struct shell *shell, size_t argc, char **argv)
{
	struct discover_ctx d = {0};
	struct dns_resolve_context *ctx = dns_resolve_get_default();
	int32_t t = CONFIG_NET_SOCKETS_DNS_TIMEOUT;
	uint16_t dns_id;
	int ret;
	int dect_idx;

	ARG_UNUSED(shell);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	dect_idx = net_if_get_by_name("dect0");
	d.dect0 = (dect_idx < 0) ? NULL : net_if_get_by_index(dect_idx);

	desh_print("");
	desh_print("dect discover: mDNS PTR _dect-nr._udp, then AAAA");
	desh_print("  ula (on-link): from L2 ULA /96 + long_rd + LL tail (\"-\" if unavailable)");
	desh_print("  discovery host: %s.local", net_hostname_get());
	desh_print("  (PTR browse may take up to ~%u s per service.)",
		   (unsigned int)(((uint32_t)discover_ptr_dns_budget_ms() + DISCOVER_PTR_SLACK_MS +
			       999U) /
			      1000U));
	/* De-synchronize concurrent discovers on different FTs (reduces mDNS collisions). */
	k_sleep(K_MSEC(50 + (sys_rand32_get() % 450U)));

	desh_print("  _dect-nr._udp.local");
	ret = discover_run_ptr(ctx, &d, "_dect-nr._udp.local");
	if (ret < 0) {
		desh_error("discover: PTR _dect-nr failed or timed out (%d)", ret);
		desh_print("");
		return;
	}

	if (d.n_hosts == 0) {
		desh_print("  (no PTR instances on first pass — second browse after backoff)");
		k_sleep(K_MSEC(600 + (sys_rand32_get() % 400U)));
		ret = discover_run_ptr(ctx, &d, "_dect-nr._udp.local");
		if (ret < 0) {
			desh_error("discover: PTR _dect-nr (2nd pass) failed (%d)", ret);
			desh_print("");
			return;
		}
		if (d.n_hosts == 0) {
			desh_print("  (no _dect-nr instances found)");
			desh_print("");
			return;
		}
	}

	desh_print("  PTR pass done, %zu host(s) — resolving AAAA", d.n_hosts);
	desh_print("  AAAA %zu host(s):", d.n_hosts);
	desh_print(" %2s | %-*s | %-*s | %-*s | %-*s | long_rd_id", "#", DECT_DISCOVER_HOST_W,
		   "host", DECT_DISCOVER_KIND_W, "kind", DECT_DISCOVER_IPV6_W, "ipv6 (mDNS)",
		   DECT_DISCOVER_IPV6_W, "ula (on-link)");
	discover_print_sep_row();

	for (size_t i = 0; i < d.n_hosts; i++) {
		d.got_aaaa = false;
		d.row_index = i + 1U;
		strncpy(d.cur_host, d.hosts[i], sizeof(d.cur_host));
		d.cur_host[sizeof(d.cur_host) - 1] = '\0';

		k_sem_init(&d.wait, 0, 1);
		/* Unqualified name hits dns_resolve_name_internal() hostname shortcut (no mDNS). */
		const char *aaaa_qname = discover_host_is_self_local(
			d.cur_host) ? net_hostname_get() : d.cur_host;

		ret = dns_resolve_name(
			ctx, aaaa_qname, DNS_QUERY_TYPE_AAAA, &dns_id, aaaa_cb, &d, t);
		if (ret < 0) {
			desh_error("discover: AAAA for %.60s failed (%d)", d.cur_host, ret);
			continue;
		}
		ret = discover_wait_done(ctx, dns_id, &d.wait, aaaa_qname, DNS_QUERY_TYPE_AAAA, t,
					 DISCOVER_AAAA_SLACK_MS);
		if (ret < 0) {
			desh_warn("discover: AAAA for %.60s timed out", d.cur_host);
		}
	}

	desh_print("");
	desh_print("dect discover: finished (%zu host(s))", d.n_hosts);
	desh_print("");
}

SHELL_SUBCMD_ADD((dect), discover, NULL,
		 "Browse mDNS for _dect-nr; print peers (all AAAA: LL/ULA/GUA when "
		 "stack multi-AAAA is on, on-link ULA when ULA is configured on dect0, long RD ID). "
		 "Usage: dect discover\n",
		 dect_shell_discover_cmd, 1, 0);
