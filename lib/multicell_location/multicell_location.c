/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <stdio.h>
#if defined(CONFIG_POSIX_API)
#include <posix/arpa/inet.h>
#include <posix/unistd.h>
#include <posix/netdb.h>
#include <posix/sys/socket.h>
#else
#include <net/socket.h>
#endif
#include <modem/lte_lc.h>
#include <net/tls_credentials.h>
#include <modem/modem_key_mgmt.h>
#include <net/multicell_location.h>

#include "location_service.h"

#include <logging/log.h>

#define HTTPS_PORT	CONFIG_MULTICELL_LOCATION_HTTPS_PORT

LOG_MODULE_REGISTER(multicell_location, CONFIG_MULTICELL_LOCATION_LOG_LEVEL);

BUILD_ASSERT(!IS_ENABLED(CONFIG_MULTICELL_LOCATION_SERVICE_NONE),
	     "A location service must be enabled");

static char http_request[CONFIG_MULTICELL_LOCATION_SEND_BUF_SIZE];
static char recv_buf[CONFIG_MULTICELL_LOCATION_RECV_BUF_SIZE];

static int tls_setup(int fd, const char *hostname, enum multicell_location_service_id used_service)
{
	int err;
	int verify = TLS_PEER_VERIFY_REQUIRED;
	/* Security tag that we have provisioned the certificate to */

	const sec_tag_t tls_sec_here_tag[] = {
		CONFIG_MULTICELL_LOCATION_HERE_TLS_SEC_TAG,

	};
	const sec_tag_t tls_sec_skyhook_tag[] = {
		CONFIG_MULTICELL_LOCATION_SKYHOOK_TLS_SEC_TAG,

	};
	const sec_tag_t tls_sec_nrfcloud_tag[] = {
		CONFIG_MULTICELL_LOCATION_NRF_CLOUD_TLS_SEC_TAG,

	};
	const sec_tag_t *used_tls_sec_tag_list;

	switch (used_service) {
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_NRF_CLOUD)
	case MULTICELL_LOCATION_SERV_NRFCLOUD:
		used_tls_sec_tag_list = tls_sec_nrfcloud_tag;
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_HERE)
	case MULTICELL_LOCATION_SERV_HERE:
		used_tls_sec_tag_list = tls_sec_here_tag;
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_SKYHOOK)
	case MULTICELL_LOCATION_SERV_SKYHOOK:
		used_tls_sec_tag_list = tls_sec_skyhook_tag;
		break;
#endif
	default:
		LOG_ERR("Unknown service, used_service %d", used_service);
		return -EINVAL;
	}

	err = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err) {
		LOG_ERR("Failed to setup peer verification, err %d", errno);
		return -errno;
	}

	/* Associate the socket with the security tag
	 * we have provisioned the certificate with.
	 */
	err = setsockopt(fd, SOL_TLS, TLS_SEC_TAG_LIST, used_tls_sec_tag_list,
			 sizeof(sec_tag_t) * ARRAY_SIZE(tls_sec_here_tag));
	if (err) {
		LOG_ERR("Failed to setup TLS sec tag, error: %d", errno);
		return -errno;
	}

	err = setsockopt(fd, SOL_TLS, TLS_HOSTNAME, hostname, strlen(hostname));
	if (err < 0) {
		LOG_ERR("Failed to set hostname option, errno: %d", errno);
		return -errno;
	}

	return 0;
}

static int execute_http_request(const char *request, size_t request_len,
	enum multicell_location_service_id used_service)
{
	int err, fd, bytes;
	size_t offset = 0;
	struct addrinfo *res;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	const char *hostname;

	switch (used_service) {
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_NRF_CLOUD)
	case MULTICELL_LOCATION_SERV_NRFCLOUD:
		hostname = location_service_get_hostname_nrfcloud();
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_HERE)
	case MULTICELL_LOCATION_SERV_HERE:
		hostname = location_service_get_hostname_here();
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_SKYHOOK)
	case MULTICELL_LOCATION_SERV_SKYHOOK:
		hostname = location_service_get_hostname_skyhook();
		break;
#endif
	default:
		LOG_ERR("No hostname for used_service: %d", used_service);
		return -EINVAL;
	}

	err = getaddrinfo(hostname, NULL, &hints, &res);
	if (err) {
		LOG_ERR("getaddrinfo() failed, error: %d", err);
		return err;
	}

	if (IS_ENABLED(CONFIG_MULTICELL_LOCATION_LOG_LEVEL_DBG)) {
		char ip[INET6_ADDRSTRLEN];

		inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr,
			  ip, sizeof(ip));
		LOG_DBG("IP address: %s", log_strdup(ip));
	}

	((struct sockaddr_in *)res->ai_addr)->sin_port = htons(HTTPS_PORT);

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
	if (fd == -1) {
		LOG_ERR("Failed to open socket, errno: %d", errno);
		err = -errno;
		goto clean_up;
	}

	/* Setup TLS socket options */
	err = tls_setup(fd, hostname, used_service);
	if (err) {
		goto clean_up;
	}

	if (CONFIG_MULTICELL_LOCATION_SEND_TIMEOUT > 0) {
		struct timeval timeout = {
			.tv_sec = CONFIG_MULTICELL_LOCATION_SEND_TIMEOUT,
		};

		err = setsockopt(fd,
				 SOL_SOCKET,
				 SO_SNDTIMEO,
				 &timeout,
				 sizeof(timeout));
		if (err) {
			LOG_ERR("Failed to setup socket send timeout, errno %d", errno);
			err = -errno;
			goto clean_up;
		}
	}

	if (CONFIG_MULTICELL_LOCATION_RECV_TIMEOUT > 0) {
		struct timeval timeout = {
			.tv_sec = CONFIG_MULTICELL_LOCATION_RECV_TIMEOUT,
		};

		err = setsockopt(fd,
				 SOL_SOCKET,
				 SO_RCVTIMEO,
				 &timeout,
				 sizeof(timeout));
		if (err) {
			LOG_ERR("Failed to setup socket receive timeout, errno %d", errno);
			err = -errno;
			goto clean_up;
		}
	}

	err = connect(fd, res->ai_addr, sizeof(struct sockaddr_in));
	if (err) {
		LOG_ERR("connect() failed, errno: %d", errno);
		err = -errno;
		goto clean_up;
	}

	do {
		bytes = send(fd, &request[offset], request_len - offset, 0);
		if (bytes < 0) {
			LOG_ERR("send() failed, errno: %d", errno);
			err = -errno;
			goto clean_up;
		}

		offset += bytes;
	} while (offset < request_len);

	LOG_DBG("Sent %d bytes", offset);

	offset = 0;

	do {
		bytes = recv(fd, &recv_buf[offset], sizeof(recv_buf) - offset - 1, 0);
		if (bytes < 0) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == ETIMEDOUT)) {
				LOG_WRN("Receive timeout, possibly incomplete data received");

				/* It's been observed that some services seemingly doesn't
				 * close the connection as expected, causing recv() to never
				 * return 0. In these cases we may have received all data,
				 * so we choose to break here to continue parsing while
				 * propagating the timeout information.
				 */
				err = -ETIMEDOUT;
				break;
			}

			LOG_ERR("recv() failed, errno: %d", errno);

			err = -errno;
			goto clean_up;
		} else {
			LOG_DBG("Received HTTP response chunk of %d bytes", bytes);
		}

		offset += bytes;
	} while (bytes != 0);

	recv_buf[offset] = '\0';

	LOG_DBG("Received %d bytes", offset);

	if (offset > 0) {
		LOG_DBG("HTTP response:\n%s\n", log_strdup(recv_buf));
	}

	LOG_DBG("Closing socket");

	/* Propagate timeout information */
	if (err != -ETIMEDOUT) {
		err = 0;
	}

clean_up:
	freeaddrinfo(res);
	(void)close(fd);

	return err;
}

int multicell_location_get(const struct lte_lc_cells_info *cell_data,
			   struct multicell_location *location,
			   enum multicell_location_service_id used_service)
{
	int err;

	if ((cell_data == NULL) || (location == NULL)) {
		return -EINVAL;
	}

	if (cell_data->ncells_count > CONFIG_MULTICELL_LOCATION_MAX_NEIGHBORS) {
		LOG_WRN("Found %d neighbor cells, but %d cells will be used in location request",
			cell_data->ncells_count, CONFIG_MULTICELL_LOCATION_MAX_NEIGHBORS);
		LOG_WRN("Increase CONFIG_MULTICELL_LOCATION_MAX_NEIGHBORS to use more cells");
	}

	switch (used_service) {
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_NRF_CLOUD)
	case MULTICELL_LOCATION_SERV_NRFCLOUD:
		err = location_service_generate_request_nrfcloud(
			cell_data, http_request, sizeof(http_request));
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_HERE)
	case MULTICELL_LOCATION_SERV_HERE:
		err = location_service_generate_request_here(
			cell_data, http_request, sizeof(http_request));
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_SKYHOOK)
	case MULTICELL_LOCATION_SERV_SKYHOOK:
		err = location_service_generate_request_skyhook(
			cell_data, http_request, sizeof(http_request));
		break;
#endif
	default:
		LOG_ERR("Unknown service, service: %d", used_service);
		err = -EINVAL;
		break;
	}

	if (err) {
		LOG_ERR("Failed to generate HTTP request, error: %d", err);
		return err;
	}

	LOG_DBG("Generated request:\n%s", log_strdup(http_request));

	err = execute_http_request(http_request, strlen(http_request), used_service);
	if (err == -ETIMEDOUT) {
		LOG_WRN("Data reception timed out, attempting to parse possibly incomplete data");
	} else if (err) {
		LOG_ERR("HTTP request failed, error: %d", err);
		return err;
	}

	switch (used_service) {
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_NRF_CLOUD)
	case MULTICELL_LOCATION_SERV_NRFCLOUD:
		err = location_service_parse_response_nrfcloud(recv_buf,
							       location);
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_HERE)
	case MULTICELL_LOCATION_SERV_HERE:
		err = location_service_parse_response_here(recv_buf, location);
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_SKYHOOK)
	case MULTICELL_LOCATION_SERV_SKYHOOK:
		err = location_service_parse_response_skyhook(recv_buf,
							      location);
		break;
#endif
	default:
		LOG_ERR("Unknown service %d to parse http response", used_service);
		err = -EINVAL;
		break;
	}

	if (err) {
		LOG_ERR("Failed to parse HTTP response");
		return -ENOMSG;
	}

	return 0;
}

int multicell_location_provision_certificate(bool overwrite,
	enum multicell_location_service_id used_service)
{
	int err;
	bool exists;
	uint8_t unused;
	nrf_sec_tag_t used_sec_tag;
	const char *certificate = NULL;

	switch (used_service) {
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_NRF_CLOUD)
	case MULTICELL_LOCATION_SERV_NRFCLOUD:
		certificate = location_service_get_certificate_nrfcloud();
		used_sec_tag = CONFIG_MULTICELL_LOCATION_NRF_CLOUD_TLS_SEC_TAG;
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_HERE)
	case MULTICELL_LOCATION_SERV_HERE:
		certificate = location_service_get_certificate_here();
		used_sec_tag = CONFIG_MULTICELL_LOCATION_HERE_TLS_SEC_TAG;
		break;
#endif
#if defined(CONFIG_MULTICELL_LOCATION_SERVICE_SKYHOOK)
	case MULTICELL_LOCATION_SERV_SKYHOOK:
		certificate = location_service_get_certificate_skyhook();
		used_sec_tag = CONFIG_MULTICELL_LOCATION_SKYHOOK_TLS_SEC_TAG;
		break;
#endif
	default:
		LOG_ERR("No certificate for service %d", used_service);
		certificate = NULL;
		break;
	}

	if (certificate == NULL) {
		LOG_ERR("No certificate was provided by the location service");
		return -EFAULT;
	}

	err = modem_key_mgmt_exists(used_sec_tag,
				    MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				    &exists, &unused);
	if (err) {
		LOG_ERR("Failed to check for certificates err %d", err);
		return err;
	}

	if (exists && overwrite) {
		/* For the sake of simplicity we delete what is provisioned
		 * with our security tag and reprovision our certificate.
		 */
		err = modem_key_mgmt_delete(used_sec_tag,
					    MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
		if (err) {
			LOG_ERR("Failed to delete existing certificate, err %d", err);
		}
	} else if (exists && !overwrite) {
		LOG_INF("A certificate is already provisioned to sec tag %d",
			used_sec_tag);
		return 0;
	}

	LOG_INF("Provisioning certificate");

	/*  Provision certificate to the modem */
	err = modem_key_mgmt_write(used_sec_tag,
				   MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				   certificate, strlen(certificate));
	if (err) {
		LOG_ERR("Failed to provision certificate, err %d", err);
		return err;
	}

	return 0;
}
