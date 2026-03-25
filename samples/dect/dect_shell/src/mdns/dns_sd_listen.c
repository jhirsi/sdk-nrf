/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Minimal UDP bind so Zephyr's mDNS DNS-SD code advertises this service in
 * service-type enumeration (e.g. avahi-browse). The stack checks that the
 * advertised port is bound (see dns_sd.c: port_in_use) before answering PTR
 * queries for _services._dns-sd._udp.local.
 *
 * UDP is used so CONFIG_NET_TCP is not required (IPv6-only DECT builds often
 * disable TCP; SOCK_STREAM then fails with errno 107 EPROTOTYPE).
 *
 * DNS-SD instance name follows the current hostname (net_hostname_get), updated
 * via dect_shell_dns_sd_refresh() after bind and when the hostname shell command
 * changes the name. Instance labels are truncated to DNS_SD_INSTANCE_MAX_SIZE
 * (63) per RFC 6763 if NET_HOSTNAME_MAX_LEN is larger.
 *
 * The socket is bound with SO_BINDTODEVICE to the DECT interface after it is
 * NET_IF_UP so dns_sd port_in_use checks and mDNS replies align with dect0.
 * Binding before DECT is up can prevent peers from answering _services._dns-sd
 * browse while hostname (AAAA) answers still work from the stack.
 *
 * mDNS queries stay on link-scope multicast (ff02::fb). The stack advertises all
 * suitable unicast IPv6 addresses on dect0 in DNS-SD PTR responses (link-local,
 * ULA when CONFIG_NET_L2_DECT_ULA installs it, etc.). A net_mgmt hook refreshes
 * DNS-SD records on NET_EVENT_IPV6_ADDR_ADD/DEL on dect0 so answers track addresses.
 *
 * Built only when CONFIG_DECT_SHELL_MDNS_DNS_SD_ADVERTISE is enabled (depends on
 * CONFIG_MDNS_RESPONDER_DNS_SD and related mDNS/DNS-SD Kconfig).
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dns_sd.h>
#include <zephyr/net/hostname.h>
#include <zephyr/net/mdns_responder.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(dect_shell_dns_sd, LOG_LEVEL_INF);

/* DECT L2 net_if name from Zephyr (see "net iface" in shell). */
#define DECT_SHELL_MDNS_IFACE_NAME "dect0"

static char dect_shell_dns_sd_instance[DNS_SD_INSTANCE_MAX_SIZE + 1];

static const uint16_t dect_shell_dns_sd_port_be =
	sys_cpu_to_be16((uint16_t)CONFIG_DECT_SHELL_MDNS_DNS_SD_PORT);

static struct dns_sd_rec dect_shell_dns_sd_rec = {
	.instance = dect_shell_dns_sd_instance,
	.service = "_dect-nr",
	.proto = "_udp",
	.domain = "local",
	.text = DNS_SD_EMPTY_TXT,
	.text_size = 0,
	.port = &dect_shell_dns_sd_port_be,
};

static K_MUTEX_DEFINE(dect_shell_dns_sd_mutex);

void dect_shell_dns_sd_refresh(void)
{
	const char *h = net_hostname_get();
	size_t n = strlen(h);

	if (n > DNS_SD_INSTANCE_MAX_SIZE) {
		n = DNS_SD_INSTANCE_MAX_SIZE;
	}
	if (n < DNS_SD_INSTANCE_MIN_SIZE) {
		LOG_WRN("DNS-SD: hostname too short for service instance, skip");
		return;
	}

	k_mutex_lock(&dect_shell_dns_sd_mutex, K_FOREVER);
	memcpy(dect_shell_dns_sd_instance, h, n);
	dect_shell_dns_sd_instance[n] = '\0';
	(void)mdns_responder_set_ext_records(&dect_shell_dns_sd_rec, 1);
	k_mutex_unlock(&dect_shell_dns_sd_mutex);
}

static struct net_if *dect_shell_mdns_net_if(void)
{
	int idx = net_if_get_by_name(DECT_SHELL_MDNS_IFACE_NAME);

	if (idx < 0) {
		return NULL;
	}

	return net_if_get_by_index(idx);
}

static bool dect_mdns_iface_is_up(void)
{
	struct net_if *iface = dect_shell_mdns_net_if();

	return iface != NULL && net_if_flag_is_set(iface, NET_IF_UP);
}

static void dns_sd_listen_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int srv;
	struct sockaddr_in6 bind_addr = { 0 };
	uint8_t drain[128];
	struct net_if *dect_iface;
	struct net_ifreq if_req;
	char ifname[CONFIG_NET_INTERFACE_NAME_LEN + 1];

	bind_addr.sin6_family = AF_INET6;
	bind_addr.sin6_port = htons(CONFIG_DECT_SHELL_MDNS_DNS_SD_PORT);

	for (;;) {
		if (!dect_mdns_iface_is_up()) {
			k_sleep(K_MSEC(500));
			continue;
		}

		dect_iface = dect_shell_mdns_net_if();
		if (dect_iface == NULL) {
			k_sleep(K_MSEC(500));
			continue;
		}

		srv = zsock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		if (srv < 0) {
			LOG_ERR("DNS-SD stub: socket failed (%d)", errno);
			k_sleep(K_SECONDS(2));
			continue;
		}

		memset(&if_req, 0, sizeof(if_req));
		if (net_if_get_name(dect_iface, ifname, sizeof(ifname) - 1) >= 0) {
			memcpy(if_req.ifr_name, ifname,
			       MIN(sizeof(ifname) - 1, sizeof(if_req.ifr_name) - 1));
			if (zsock_setsockopt(srv, ZSOCK_SOL_SOCKET, ZSOCK_SO_BINDTODEVICE, &if_req,
					     sizeof(if_req)) < 0) {
				LOG_WRN("DNS-SD stub: SO_BINDTODEVICE failed (%d)", errno);
			}
		}

		if (zsock_bind(srv, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
			LOG_WRN("DNS-SD stub: bind UDP %u failed (%d), retry",
				(unsigned int)CONFIG_DECT_SHELL_MDNS_DNS_SD_PORT, errno);
			zsock_close(srv);
			k_sleep(K_SECONDS(2));
			continue;
		}

		LOG_INF("DNS-SD stub: bound UDP %u on %s (_dect-nr._udp), instance=%s",
			(unsigned int)CONFIG_DECT_SHELL_MDNS_DNS_SD_PORT,
			DECT_SHELL_MDNS_IFACE_NAME,
			net_hostname_get());
		break;
	}

	dect_shell_dns_sd_refresh();

	/* Drain datagrams sent to CONFIG_DECT_SHELL_MDNS_DNS_SD_PORT (default 4700).
	 * Standard mDNS (UDP 5353) is handled inside the stack (mdns_responder), not here.
	 * This loop does not hold net_pkt/net_buf beyond zsock_recv(); pool pressure during
	 * e.g. avahi-browse is from 5353 + DECT RX paths, not from this stub port.
	 */
	for (;;) {
		ssize_t n = zsock_recv(srv, drain, sizeof(drain), 0);

		if (n < 0) {
			LOG_WRN("DNS-SD stub: recv failed (%d)", errno);
			k_sleep(K_MSEC(200));
		}
	}
}

static void dect_shell_dns_sd_ipv6_evt(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				       struct net_if *iface)
{
	ARG_UNUSED(cb);

	if (mgmt_event != NET_EVENT_IPV6_ADDR_ADD && mgmt_event != NET_EVENT_IPV6_ADDR_DEL) {
		return;
	}

	if (iface != dect_shell_mdns_net_if()) {
		return;
	}

	dect_shell_dns_sd_refresh();
}

static struct net_mgmt_event_callback dect_shell_dns_sd_ipv6_cb;

static int dect_shell_dns_sd_net_mgmt_init(void)
{
	net_mgmt_init_event_callback(&dect_shell_dns_sd_ipv6_cb, dect_shell_dns_sd_ipv6_evt,
				     NET_EVENT_IPV6_ADDR_ADD | NET_EVENT_IPV6_ADDR_DEL);
	net_mgmt_add_event_callback(&dect_shell_dns_sd_ipv6_cb);
	return 0;
}

SYS_INIT(dect_shell_dns_sd_net_mgmt_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

K_THREAD_DEFINE(dect_shell_dns_sd_listen, 2048, dns_sd_listen_thread, NULL, NULL, NULL,
		K_PRIO_COOP(7), 0, 0);
