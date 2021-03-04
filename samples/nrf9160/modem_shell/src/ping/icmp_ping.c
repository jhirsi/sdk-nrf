/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <string.h>

#include <zephyr.h>

#include <modem/modem_info.h>

#include <posix/unistd.h>
#include <posix/netdb.h>

#include <posix/poll.h>
#include <posix/sys/socket.h>

#include <posix/arpa/inet.h>

#include "utils/fta_net_utils.h"
#include "ltelc_api.h"

#include "icmp_ping.h"

#define ICMP    1               // Protocol
#define ICMP6   6
#define ICMPV6  58              // Next Header
#define IP_NEXT_HEADER_POS  6   // Next Header
#define IP_PROTOCOL_POS     9   // Protocol
#define ICMP_ECHO_REP       0
#define ICMP_ECHO_REQ       8
#define ICMP6_ECHO_REQ      128
#define ICMP6_ECHO_REP      129

/**@ ICMP Ping command arguments */
static icmp_ping_shell_cmd_argv_t ping_argv;

/* global variable defined in different files */
extern struct modem_param_info modem_param;

/*****************************************************************************/
static inline void setip(uint8_t *buffer, uint32_t ipaddr)
{
	buffer[0] = ipaddr & 0xFF;
	buffer[1] = (ipaddr >> 8) & 0xFF;
	buffer[2] = (ipaddr >> 16) & 0xFF;
	buffer[3] = ipaddr >> 24;
}
/*****************************************************************************/
static uint16_t check_ics(const uint8_t *buffer, int len)
{
	const uint32_t *ptr32 = (const uint32_t *)buffer;
	uint32_t hcs = 0;
	const uint16_t *ptr16;

	for (int i = len / 4; i > 0; i--) {
		uint32_t s = *ptr32++;

		hcs += s;
		if (hcs < s) {
			hcs++;
		}
	}

	ptr16 = (const uint16_t *)ptr32;

	if (len & 2) {
		uint16_t s = *ptr16++;

		hcs += s;
		if (hcs < s) {
			hcs++;
		}
	}

	if (len & 1) {
		const uint8_t *ptr8 = (const uint8_t *)ptr16;
		uint8_t s = *ptr8;

		hcs += s;
		if (hcs < s) {
			hcs++;
		}
	}

	while (hcs > 0xFFFF) {
		hcs = (hcs & 0xFFFF) + (hcs >> 16);
	}

	return ~hcs; /* One's complement */
}
/*****************************************************************************/
static void calc_ics(uint8_t *buffer, int len, int hcs_pos)
{
	uint16_t *ptr_hcs = (uint16_t *)(buffer + hcs_pos);
	*ptr_hcs = 0; /* Clear checksum before calculation */
	uint16_t hcs;

	hcs = check_ics(buffer, len);
	*ptr_hcs = hcs;
}
/*****************************************************************************/
static uint32_t send_ping_wait_reply(const struct shell *shell)
{
	static int64_t start_t;
	int64_t delta_t;
	int32_t timeout;
	static uint8_t seqnr = 0;
	uint16_t total_length;
	uint8_t *buf = NULL;
	uint8_t *data = NULL;
	uint8_t rep = 0;
	uint8_t header_len = 0;
	struct addrinfo *si = ping_argv.src;
	const int alloc_size = ICMP_LINK_MTU;
  	struct pollfd fds[1];
	int dpllen, pllen, len;
	int fd;
	int plseqnr;
	int ret;
	const uint16_t icmp_hdr_len = ICMP_HDR_LEN;

    if (si->ai_family == AF_INET)
    {
        // Generate IPv4 ICMP EchoReq

        // Ping header
        header_len = ICMP_IPV4_HDR_LEN;

        total_length = ping_argv.len + header_len + icmp_hdr_len;

        buf = calloc(1, alloc_size);

		if (buf == NULL) {
			shell_error(shell, "No RAM memory available for sending ping.");
			return -1;
		}

        buf[0] = (4 << 4) + (header_len / 4);   // Version & header length
        //buf[1] = 0;                           // Type of service
        buf[2] = total_length >> 8;             // Total length
        buf[3] = total_length & 0xFF;           // Total length
        //buf[4..5] = 0;                        // Identification
        //buf[6..7] = 0;                        // Flags & fragment offset
        buf[8] = 64;                            // TTL
        buf[9] = ICMP;                          // Protocol
        //buf[10..11] = ICS, calculated later

        struct sockaddr_in *sa = (struct sockaddr_in *)ping_argv.src->ai_addr;
        setip(buf+12, sa->sin_addr.s_addr);     // Source
        sa = (struct sockaddr_in *)ping_argv.dest->ai_addr;
        setip(buf+16, sa->sin_addr.s_addr);     // Destination

        calc_ics(buf, header_len, 10);

        // ICMP header
        data = buf + header_len;
        data[0] = ICMP_ECHO_REQ;                // Type (echo req)
        //data[1] = 0;                          // Code
        //data[2..3] = checksum, calculated later
        //data[4..5] = 0;                       // Identifier
        //data[6] = 0;                          // seqnr >> 8
        data[7] = ++seqnr;

        // Payload
        for (int i = 8; i < total_length - header_len; i++)
        {
            data[i] = (i + seqnr) % 10 + '0';
        }

        // ICMP CRC
        calc_ics(data, total_length - header_len, 2);

        rep = ICMP_ECHO_REP;
    }
	else
	{
        // Generate IPv6 ICMP EchoReq

        // ipv6 header
        header_len = ICMP_IPV6_HDR_LEN;
        uint16_t payload_length = ping_argv.len + icmp_hdr_len;

        total_length = payload_length + header_len;
        buf = calloc(1, alloc_size);
		
		if (buf == NULL) {
			shell_error(shell, "No RAM memory available for sending ping.");
			return -1;
		}

        buf[0] = (6 << 4);                      // Version & traffic class 4 bits
        //buf[1..3] = 0;                        // Traffic class 4 bits & flow label
        buf[4] = payload_length >> 8;           // Payload length
        buf[5] = payload_length & 0xFF;         // Payload length
        buf[6] = ICMPV6;                        // Next header (58)
        buf[7] = 64;                            // Hop limit

        struct sockaddr_in6 *sa = (struct sockaddr_in6 *)ping_argv.src->ai_addr;
        memcpy(buf + 8, sa->sin6_addr.s6_addr, 16);     // Source address

        sa = (struct sockaddr_in6 *)ping_argv.dest->ai_addr;
        memcpy(buf + 24, sa->sin6_addr.s6_addr, 16);    // Destination address

        // ICMPv6 header
        data = buf + header_len;
        data[0] = ICMP6_ECHO_REQ;               // Type (echo req)
        //data[1] = 0;                          // Code
        //data[2..3] = checksum, calculated later
        //data[4..5] = 0;                       // Identifier
        //data[6] = 0;                          // seqnr >> 8
        data[7] = ++seqnr;

        // Payload
        for (int i = 0; i < ping_argv.len; i++)
        {
            data[i + icmp_hdr_len] = (i + seqnr) % 10 + '0';
        }
        
		// ICMPv6 CRC: https://tools.ietf.org/html/rfc4443#section-2.3
		// for IPv6 pseudo header see: https://tools.ietf.org/html/rfc2460#section-8.1
        uint32_t hcs = check_ics(buf + 8, 32);  // Pseudo header: source + dest
        hcs += check_ics(buf + 4, 2);           // Pseudo header: payload length

        uint8_t tbuf[2];
        tbuf[0] = 0; tbuf[1] = buf[6];

        hcs += check_ics(tbuf, 2);              // Pseudo header: Next header
        hcs += check_ics(data, 2);                       //ICMP: Type & Code
		hcs += check_ics(data + 4,  4 + ping_argv.len);  //ICMP: Header data + Data

        while(hcs > 0xFFFF)
            hcs = (hcs & 0xFFFF) + (hcs >> 16);

        data[2] = hcs & 0xFF;
        data[3] = hcs >> 8;

        rep = ICMP6_ECHO_REP;
	}

	/* Send the ping */
	errno = 0;
	delta_t = 0;

	fd = socket(AF_PACKET, SOCK_RAW, 0);
	if (fd < 0) {
		shell_error(shell, "socket() failed: (%d)", -errno);
	    free(buf);
		return (uint32_t)delta_t;
	}
	if (ping_argv.cid != FTA_ARG_NOT_SET) {
		/* Binding a data socket to an APN: */
		ret = fta_net_utils_socket_apn_set(fd, ping_argv.current_apn_str);
		if (ret != 0) {
			shell_error(shell, "Cannot bind socket to apn %s", ping_argv.current_apn_str);
			shell_error(shell, "probably due to https://projecttools.nordicsemi.no/jira/browse/NCSDK-6645");
			goto close_end;
		}			
	}

	ret = send(fd, buf, total_length, 0);
	if (ret <= 0) {
		shell_error(shell, "send() failed: (%d)", -errno);
		goto close_end;
	}

	start_t = k_uptime_get();
	timeout = ping_argv.timeout;

wait_for_data:
	fds[0].fd = fd;
	fds[0].events = POLLIN;
	ret = poll(fds, 1, timeout);
	if (ret == 0) {
		shell_print(shell, "Pinging %s results: request timed out",
			    ping_argv.target_name);
		goto close_end;
	} else if (ret < 0) {
		shell_error(shell, "poll() failed: (%d) (%d)", -errno, ret);
		goto close_end;
	}


	/* receive response */
	do {
		len = recv(fd, buf, alloc_size, 0);
		if (len <= 0) {
			shell_error(shell, "recv() failed: (%d) (%d)", -errno, len);
			goto close_end;
		}
		if (len < header_len) {
			/* Data length error, ignore "silently" */
			shell_error(shell, "recv() wrong data (%d)", len);
			continue;
		}
		if ((rep == ICMP_ECHO_REP && buf[IP_PROTOCOL_POS] != ICMP) || 
		    (rep == ICMP6_ECHO_REP && buf[IP_NEXT_HEADER_POS] != ICMPV6)) {
			/* Not ipv4/ipv6 echo reply, ignore silently */
			continue;
		}
		break;
	} while (1);

	delta_t = k_uptime_get() - start_t;

	if (rep == ICMP_ECHO_REP) {
		/* Check ICMP HCS */
		int hcs = check_ics(data, len - header_len);
		if (hcs != 0) {
			shell_error(shell, "IPv4 HCS error, hcs: %d, len: %d\r\n", hcs, len);
			delta_t = 0;
			goto close_end;
		}
		pllen = (buf[2] << 8) + buf[3]; // Raw socket payload length
	} else {
	        // Check ICMP6 CRC
	        uint32_t hcs = check_ics(buf + 8, 32);  // Pseudo header source + dest
	        hcs += check_ics(buf + 4, 2);           // Pseudo header packet length
	        uint8_t tbuf[2];
	        tbuf[0] = 0; tbuf[1] = buf[6];
	        hcs += check_ics(tbuf, 2);              // Pseudo header Next header
	        hcs += check_ics(data, 2);              // Type & Code
	        hcs += check_ics(data + 4, len - header_len - 4);   // Header data + Data

	        while(hcs > 0xFFFF)
	            hcs = (hcs & 0xFFFF) + (hcs >> 16);

	        int plhcs = data[2] + (data[3] << 8);
		if (plhcs != hcs) {
			shell_error(shell, "IPv6 HCS error: 0x%x 0x%x\r\n", plhcs, hcs);
			delta_t = 0;
			goto close_end;
		}
		/* Raw socket payload length */
	        pllen = (buf[4] << 8) + buf[5] + header_len; // Payload length - hdr
	}
	
	/* Data payload length: */
	dpllen = pllen - header_len - icmp_hdr_len;

	/* Check seqnr and length */
	plseqnr = data[7];
	if (plseqnr != seqnr) {
		/* This is not the reply you are looking for */

		/* Calculate how much there's still time left */
		timeout = ping_argv.timeout - (int32_t)delta_t;
		delta_t = 0;
		/* Wait for next response if there' still time */
		if (timeout > 0) {
			goto wait_for_data;
		} else {
			goto close_end;
		}
	}
	if (pllen != len) {
		shell_error(shell, "Expected length %d, got %d",
			    len, pllen);
		delta_t = 0;
		goto close_end;
	}

	/* Result */
	shell_print(shell, "Pinging %s results: time=%d.%03dsecs, payload sent: %d, payload received %d",
		ping_argv.target_name, (uint32_t)(delta_t) / 1000,
		(uint32_t)(delta_t) % 1000, ping_argv.len, dpllen);

close_end:
	(void)close(fd);
	free(buf);
	return (uint32_t)delta_t;
}
/*****************************************************************************/
static void icmp_ping_tasks_execute(const struct shell *shell)
{
	struct addrinfo *si = ping_argv.src;
	struct addrinfo *di = ping_argv.dest;
	uint32_t sum = 0;
	uint32_t count = 0;

	for (int i = 0; i < ping_argv.count; i++) {
		uint32_t ping_t = send_ping_wait_reply(shell);

		if (ping_t > 0) {
			count++;
			sum += ping_t;
		}
		k_sleep(K_MSEC(ping_argv.interval));
	}

	freeaddrinfo(si);
	freeaddrinfo(di);
	shell_print(shell, "Pinging DONE\r\n");
}
/*****************************************************************************/
int icmp_ping_start(const struct shell *shell, icmp_ping_shell_cmd_argv_t *ping_args)
{
	int st = -1;
	struct addrinfo *res;
	char src_ipv_addr[NET_IPV6_ADDR_LEN];
	char *apn = NULL;

	/* Copy args in local storage here: */
	memcpy(&ping_argv, ping_args, sizeof(icmp_ping_shell_cmd_argv_t));

	shell_print(shell, "Initiating ping to: %s", ping_argv.target_name);

	if (ping_argv.cid != FTA_ARG_NOT_SET) {
		apn = ping_argv.current_apn_str;
	}

    /* Sets getaddrinfo hints by using current host address(es): */
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_next =  apn ?
			&(struct addrinfo) {
				.ai_family    = AF_LTE,
				.ai_socktype  = SOCK_MGMT,
				.ai_protocol  = NPROTO_PDN,
				.ai_canonname = (char *)apn
			} : NULL,
	};
	
	inet_ntop(AF_INET,  &(ping_argv.current_addr4), src_ipv_addr, sizeof(src_ipv_addr));
    if (ping_argv.current_pdp_type == PDP_TYPE_IP4V6) {
		if (ping_argv.force_ipv6) {
			hints.ai_family = AF_INET6;
			inet_ntop(AF_INET6,  &(ping_argv.current_addr6), src_ipv_addr, sizeof(src_ipv_addr));
		}
	}
    if (ping_argv.current_pdp_type == PDP_TYPE_IPV6) {
		hints.ai_family = AF_INET6;
		inet_ntop(AF_INET6,  &(ping_argv.current_addr6), src_ipv_addr, sizeof(src_ipv_addr));
	}	
	st = getaddrinfo(src_ipv_addr, NULL, &hints, &res);
	if (st != 0) {
		shell_error(shell, "getaddrinfo(src) error: %d", st);
		return -st;
	}
	ping_argv.src = res;

	/* Get destination */
	res = NULL;
	st = getaddrinfo(ping_argv.target_name, NULL, &hints, &res);
	if (st != 0) {
		shell_error(shell, "getaddrinfo(dest) error: %d", st);
		shell_error(shell, "Cannot resolve remote host\r\n");
		freeaddrinfo(ping_argv.src);
		return -st;
	}
	ping_argv.dest = res;

	if (ping_argv.src->ai_family != ping_argv.dest->ai_family) {
		shell_error(shell, "Source/Destination address family error");
		freeaddrinfo(ping_argv.dest);
		freeaddrinfo(ping_argv.src);
		return -1;
	} else {
		struct sockaddr *sa;
		sa = ping_argv.src->ai_addr;
		shell_print(shell, "Source IP addr: %s", fta_net_utils_sckt_addr_ntop(sa));
		sa = ping_argv.dest->ai_addr;
		shell_print(shell, "Destination IP addr: %s",
			    fta_net_utils_sckt_addr_ntop(sa));
	}
	/* Now we can check the max paload len for IPv6: */
	if (ping_argv.src->ai_family == AF_INET6 && ping_argv.len > ICMP_IPV6_MAX_LEN) {
		shell_error(shell, "Payload size for ipv6 exceeds the limit %d %d ", ICMP_IPV6_MAX_LEN);
		return -1;
	}
 
	icmp_ping_tasks_execute(shell);
	return 0;
}
