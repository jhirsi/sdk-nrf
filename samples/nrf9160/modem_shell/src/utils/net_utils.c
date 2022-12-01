/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/netdb.h>
#include <zephyr/shell/shell.h>
#include <nrf_socket.h>
#include <nrf_modem_at.h>

#include "net_utils.h"
#include "mosh_print.h"

int net_utils_socket_pdn_id_set(int fd, uint32_t pdn_id)
{
	int ret;
	size_t len;
	struct ifreq ifr = { 0 };

	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "pdn%d", pdn_id);
	len = strlen(ifr.ifr_name);

	ret = setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, len);
	if (ret < 0) {
		mosh_error(
			"Failed to bind socket with PDN ID %d, error: %d, %s",
			pdn_id, ret, strerror(ret));
		return -EINVAL;
	}

	return 0;
}

char *net_utils_sckt_addr_ntop(const struct sockaddr *addr)
{
	static char buf[NET_IPV6_ADDR_LEN];

	if (addr->sa_family == AF_INET6) {
		return inet_ntop(AF_INET6, &net_sin6(addr)->sin6_addr, buf,
				 sizeof(buf));
	}

	if (addr->sa_family == AF_INET) {
		return inet_ntop(AF_INET, &net_sin(addr)->sin_addr, buf,
				 sizeof(buf));
	}

	strcpy(buf, "Unknown AF");
	return buf;
}

int net_utils_sa_family_from_ip_string(const char *src)
{
	char buf[INET6_ADDRSTRLEN];

	if (inet_pton(AF_INET, src, buf)) {
		return AF_INET;
	} else if (inet_pton(AF_INET6, src, buf)) {
		return AF_INET6;
	}
	return -1;
}

bool net_utils_ip_string_is_valid(const char *src)
{
	struct nrf_in6_addr in6_addr;

	/* Use nrf_inet_pton() because this has full IP address validation. */
	return (nrf_inet_pton(NRF_AF_INET, src, &in6_addr) == 1 ||
		nrf_inet_pton(NRF_AF_INET6, src, &in6_addr) == 1);
}

/******************************************************************************/

void net_utils_get_ip_addr(int cid, char *addr4, char *addr6)
{
	int ret;
	char cmd[128];
	char tmp[sizeof(struct in6_addr)];
	char addr1[NET_IPV6_ADDR_LEN] = { 0 };
	char addr2[NET_IPV6_ADDR_LEN] = { 0 };

	sprintf(cmd, "AT+CGPADDR=%d", cid);

	/** parse +CGPADDR: <cid>,<PDP_addr_1>,<PDP_addr_2>
	 * PDN type "IP": PDP_addr_1 is <IPv4>, max 16(INET_ADDRSTRLEN), '.' and digits
	 * PDN type "IPV6": PDP_addr_1 is <IPv6>, max 46(INET6_ADDRSTRLEN),':', digits, 'A'~'F'
	 * PDN type "IPV4V6": <IPv4>,<IPv6>, or <IPV4> or <IPv6>
	 */
	ret = nrf_modem_at_scanf(cmd, "+CGPADDR: %*d,\"%46[.:0-9A-F]\",\"%46[:0-9A-F]\"", addr1,
				 addr2);
	if (ret <= 0) {
		return;
	}

	/* parse 1st IP string */
	if (addr4 != NULL && inet_pton(AF_INET, addr1, tmp) == 1) {
		strcpy(addr4, addr1);
	} else if (addr6 != NULL && inet_pton(AF_INET6, addr1, tmp) == 1) {
		strcpy(addr6, addr1);
		return;
	}

	/* parse second IP string (IPv6 only) */
	if (addr6 == NULL) {
		return;
	}
	if (ret > 1 && inet_pton(AF_INET6, addr2, tmp) == 1) {
		strcpy(addr6, addr2);
	}
}
