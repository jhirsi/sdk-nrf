/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <posix/arpa/inet.h>
#include <posix/sys/socket.h>
#include <posix/netdb.h>

#include "net_utils.h"

int fta_net_utils_socket_apn_set(int fd, const char *apn)
{
	int ret;
	size_t len;
	struct ifreq ifr = {0};

	__ASSERT_NO_MSG(apn);

	len = strlen(apn);
	if (len >= sizeof(ifr.ifr_name)) {
		printf("Access point name is too long\n");
		return -EINVAL;
	}

	memcpy(ifr.ifr_name, apn, len);
	ret = setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, len);
	if (ret < 0) {
		printf("Failed to bind socket, error: %d, %s\n",  ret, strerror(ret));
		return -EINVAL;
	}

	return 0;
}

char *fta_net_utils_sckt_addr_ntop(const struct sockaddr *addr)
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

	//LOG_ERR("Unknown IP address family:%d", addr->sa_family);
	strcpy(buf, "Unknown AF");
	return buf;
}
int fta_net_utils_sa_family_from_ip_string(const char *src)
{
	char buf[INET6_ADDRSTRLEN];
	if (inet_pton(AF_INET, src, buf)) {
		//printf("fta_net_utils_sa_family_from_ip_string AF_INET");
		return AF_INET;
	} else if (inet_pton(AF_INET6, src, buf)) {
		return AF_INET6;
	}
	return -1;
}

#ifdef RM_H
static char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}
#endif
