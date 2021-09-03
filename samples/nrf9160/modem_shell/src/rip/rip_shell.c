/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <zephyr.h>
#include <assert.h>

#include <shell/shell.h>
#include <getopt.h>

#include <net/http_parser.h>
#include <net/srest_client.h>

#include "rip_shell.h"

static const char rip_shell_cmd_usage_str[] =
	"Usage: rest [optional options] -h host_to_connect -p port -m method [-b body] [-H header] [-t sec_tag]\n"
	"\n"
	"  -h, --help,              Shows this help information\n";

/* Following are not having short options: */
//TODO

/* Specifying the expected options (both long and short): */
static struct option long_options[] = {
	{ "host", required_argument, 0, 'd' },
	{ "port", required_argument, 0, 'p' },
	{ "url", required_argument, 0, 'u' },
	{ "body", required_argument, 0, 'b' },
	{ "header", required_argument, 0, 'H' },
	{ "method", required_argument, 0, 'm' },
	{ "sec_tag", required_argument, 0, 's' },
	{ 0, 0, 0, 0 }
};

static void rip_shell_print_usage(const struct shell *shell)
{
	shell_print(shell, "%s", rip_shell_cmd_usage_str);
}

/*****************************************************************************/
#define MAX_HEADERS 10

struct rip_header_list_item {
	bool in_use;
	char* header_str;
};

int rip_shell(const struct shell *shell, size_t argc, char **argv)
{
	int opt, i, j;
	int ret = 0;
	int long_index = 0;
	struct srest_req_resp_context rest_ctx = { 0 };
	struct rip_header_list_item headers[10] = { 0 };
	char *req_headers[MAX_HEADERS]; 
	bool headers_set = false;
	bool method_set = true;
	bool host_set = true;
	char response_buf[1024];

	/* Set defaults: */
	rest_ctx.keep_alive = false;
	rest_ctx.timeout_ms = 5000;
	rest_ctx.url = "/index.html";
	rest_ctx.sec_tag = SREST_CLIENT_NO_SEC;
	rest_ctx.connect_socket = SREST_CLIENT_SCKT_CONNECT;
	rest_ctx.port = 80;
	rest_ctx.body = "";

	if (argc < 3) {
		goto show_usage;
	}

	/* Start from the 1st argument */
	optind = 1;

	while ((opt = getopt_long(argc, argv, "d:p:b:H:m:s:u:", long_options, &long_index)) != -1) {
		switch (opt) {
		case 'm':
			if (strcmp(optarg, "get") == 0) {
				rest_ctx.http_method = HTTP_GET;
			} else if (strcmp(optarg, "post") == 0) {
				rest_ctx.http_method = HTTP_POST;
			} else {
				shell_error(shell, "Unsupported HTTP method");
				return -EINVAL;
			}
			method_set = true;
			break;
		case 'd':
			rest_ctx.host = optarg;
			host_set = true;
			break;
		case 'u':
			rest_ctx.url = optarg;
			break;
		case 's':
			rest_ctx.sec_tag = atoi(optarg);
			if (rest_ctx.sec_tag == 0) {
				shell_warn(
					shell,
					"sec_tag not an integer (> 0)");
				return -EINVAL;
			}
			break;

		case 'p':
			rest_ctx.port = atoi(optarg);
			if (rest_ctx.port == 0) {
				shell_warn(
					shell,
					"port not an integer (> 0)");
				return -EINVAL;
			}
			break;
		case 'b':
		//maybe we should allocate memory instead of using commandline? especially if multithreading?
			rest_ctx.body = optarg;
			break;
		case 'H': {
			bool room_available = false;

			/* Check if there are still room for additional header? */
			for (i = 0; i < MAX_HEADERS; i++) {
				if (!headers[i].in_use) {
					room_available = true;
					break;
				}
			}

			if (!room_available) {
				shell_error(shell, "There are already max number (%d) of headers", MAX_HEADERS);
				return -EINVAL;
			}

			for (i = 0; i < MAX_HEADERS; i++) {
				if (!headers[i].in_use) {
#if 0  //maybe we should allocate memory instead of using commandline? especially if multithreading?
					headers[i].header_str = k_malloc(strlen(optarg) + 1);
					if (headers[i].header_str == NULL) {
						shell_error(shell, "Cannot allocate memory for header %s", optarg);
						break;
					}
#endif
					headers[i].header_str = optarg;
					headers[i].in_use = true;
					headers_set = true;
				}
			}
			break;
		}
		case '?':
			goto show_usage;
		default:
			shell_error(shell, "Unknown option. See usage:");
			goto show_usage;
		}
	}
	/* Check that all mandatory args were given: */
	if (!(host_set && method_set)) {
		shell_error(shell, "Please, give all mandatory options");
		goto show_usage;
	}

	rest_ctx.header_fields = NULL;
	if (headers_set) {
		for (i = 0; i < MAX_HEADERS; i++) {
			for (j = 0; j < MAX_HEADERS; i++) {
				headers_set = false;
				if (headers[j].in_use) {
					headers_set = true;
					break;
				}
			}
			assert(headers_set == true);
			req_headers[i] = headers[j].header_str;
		}
		rest_ctx.header_fields = (const char **)req_headers;
	}
	rest_ctx.resp_buff = response_buf;
	rest_ctx.resp_buff_len = 1024;

	ret = srest_client_request(&rest_ctx);
	if (ret) {
		shell_error(shell, "Error %d from srest client", ret);
	} else {
		shell_print(shell, "Response:\n %s", rest_ctx.response);
	}

	return ret;

show_usage:
	/* Reset getopt for another users */
	optreset = 1;

	rip_shell_print_usage(shell);
	return ret;
}
