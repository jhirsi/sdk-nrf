/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file srest_client.h
 *
 * @brief nRF Cloud REST API.
 */
#ifndef SREST_REST_CLIENT_H__
#define SREST_REST_CLIENT_H__

#include <net/http_parser.h>

#define SREST_CLIENT_NO_SEC -1 /* No TLS */
#define SREST_CLIENT_SCKT_CONNECT -1 /* sREST client lib does a sckt connection */

/** @brief Parameters and data for using the sREST client API */
struct srest_req_resp_context {
	/** Connection socket; initialize to -1 and library
	 * will make the connection.
	 */
	int connect_socket;

	/** If the connection should remain after API call */
	bool keep_alive;

	/* Request: */

	int sec_tag; /* -1: TLS not used */

	/** Used HTTP method */
	enum http_method http_method;

	/** Hostname to be used in the request */
	const char *host;

	/** Port number to be used in the request */
	uint16_t port;

	/** The URL for this request, for example: /index.html */
	const char *url;

	/** The HTTP header fields. This is a NULL terminated list of header fields. */
	const char **header_fields;

	/** Payload/body, may be NULL */
	char *body;

	/* Response: */

	/** Timeout value for receiving response data */
	int32_t timeout_ms;

	/** User allocated buffer for receiving API response.
	 */
	char *resp_buff;
	/** Size of resp_buff */
	size_t resp_buff_len;

	/** Start of response data in resp_buff */
	char *response;

	/** Length of response data */
	size_t response_len;

	/** Numeric HTTP status code which corresponds to the
	 * textual description.
	 */
	uint16_t http_status_code;
};

int srest_client_request(struct srest_req_resp_context *req_ctx);

#endif /* SREST_REST_CLIENT_H__ */
