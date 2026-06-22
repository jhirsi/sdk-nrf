/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Same pattern as samples/dect/dect_bridge_routing/src/mdns_dns_sd_listen.c and
 * samples/dect/dect_shell/src/mdns/dns_sd_listen.c: minimal UDP bind on dect0 so
 * Zephyr DNS-SD can advertise _dect-nr._udp (port must be bound). mDNS on UDP 5353
 * is handled by the stack (mdns_responder). IPv6 ADDR_ADD/DEL on dect0 triggers a
 * DNS-SD refresh so records track address changes after peer sync.
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

LOG_MODULE_REGISTER(bh6_mdns, CONFIG_DECT_TETHER_IPV6_SAMPLE_LOG_LEVEL);

#define BH6_MDNS_IFACE_NAME "dect0"

static char bh6_dns_sd_instance[DNS_SD_INSTANCE_MAX_SIZE + 1];

static const uint16_t bh6_dns_sd_port_be =
	sys_cpu_to_be16((uint16_t)CONFIG_DECT_TETHER_IPV6_SAMPLE_MDNS_DNS_SD_PORT);

static struct dns_sd_rec bh6_dns_sd_rec = {
	.instance = bh6_dns_sd_instance,
	.service = "_dect-nr",
	.proto = "_udp",
	.domain = "local",
	.text = DNS_SD_EMPTY_TXT,
	.text_size = 0,
	.port = &bh6_dns_sd_port_be,
};

static K_MUTEX_DEFINE(bh6_dns_sd_mutex);

static void bh6_dns_sd_refresh(void)
{
	const char *h = net_hostname_get();
	size_t n = strlen(h);

	if (n > DNS_SD_INSTANCE_MAX_SIZE) {
		n = DNS_SD_INSTANCE_MAX_SIZE;
	}
	if (n < DNS_SD_INSTANCE_MIN_SIZE) {
		LOG_WRN("DNS-SD: hostname too short for instance label, skip");
		return;
	}

	k_mutex_lock(&bh6_dns_sd_mutex, K_FOREVER);
	memcpy(bh6_dns_sd_instance, h, n);
	bh6_dns_sd_instance[n] = '\0';
	(void)mdns_responder_set_ext_records(&bh6_dns_sd_rec, 1);
	k_mutex_unlock(&bh6_dns_sd_mutex);
}

static struct net_if *bh6_mdns_net_if(void)
{
	int idx = net_if_get_by_name(BH6_MDNS_IFACE_NAME);

	if (idx < 0) {
		return NULL;
	}

	return net_if_get_by_index(idx);
}

static bool bh6_mdns_iface_is_up(void)
{
	struct net_if *iface = bh6_mdns_net_if();

	return iface != NULL && net_if_flag_is_set(iface, NET_IF_UP);
}

static void bh6_dns_sd_listen_thread(void *p1, void *p2, void *p3)
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
	bind_addr.sin6_port = htons(CONFIG_DECT_TETHER_IPV6_SAMPLE_MDNS_DNS_SD_PORT);

	for (;;) {
		if (!bh6_mdns_iface_is_up()) {
			k_sleep(K_MSEC(500));
			continue;
		}

		dect_iface = bh6_mdns_net_if();
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
				(unsigned int)CONFIG_DECT_TETHER_IPV6_SAMPLE_MDNS_DNS_SD_PORT,
				errno);
			zsock_close(srv);
			k_sleep(K_SECONDS(2));
			continue;
		}

		LOG_INF("DNS-SD: bound UDP %u on %s (_dect-nr._udp), instance=%s",
			(unsigned int)CONFIG_DECT_TETHER_IPV6_SAMPLE_MDNS_DNS_SD_PORT,
			BH6_MDNS_IFACE_NAME, net_hostname_get());
		break;
	}

	bh6_dns_sd_refresh();

	for (;;) {
		ssize_t n = zsock_recv(srv, drain, sizeof(drain), 0);

		if (n < 0) {
			LOG_WRN("DNS-SD stub: recv failed (%d)", errno);
			k_sleep(K_MSEC(200));
		}
	}
}

static void bh6_dns_sd_ipv6_evt(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
				struct net_if *iface)
{
	ARG_UNUSED(cb);

	if (mgmt_event != NET_EVENT_IPV6_ADDR_ADD && mgmt_event != NET_EVENT_IPV6_ADDR_DEL) {
		return;
	}

	if (iface != bh6_mdns_net_if()) {
		return;
	}

	bh6_dns_sd_refresh();
}

static struct net_mgmt_event_callback bh6_dns_sd_ipv6_cb;

static int bh6_dns_sd_net_mgmt_init(void)
{
	net_mgmt_init_event_callback(&bh6_dns_sd_ipv6_cb, bh6_dns_sd_ipv6_evt,
				     NET_EVENT_IPV6_ADDR_ADD | NET_EVENT_IPV6_ADDR_DEL);
	net_mgmt_add_event_callback(&bh6_dns_sd_ipv6_cb);
	return 0;
}

SYS_INIT(bh6_dns_sd_net_mgmt_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

K_THREAD_DEFINE(bh6_dns_sd_listen, 1536, bh6_dns_sd_listen_thread, NULL, NULL, NULL,
		K_PRIO_COOP(7), 0, 0);
