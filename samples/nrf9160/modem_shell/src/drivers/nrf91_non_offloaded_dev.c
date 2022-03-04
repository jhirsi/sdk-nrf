/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

/* Experimental non-offloading nRF91 usage */

#include <nrf_modem_limits.h>
#include <nrf_modem_os.h>
#include <errno.h>
#include <fcntl.h>
#include <init.h>
#include <net/socket_offload.h>
#include <nrf_socket.h>
#include <nrf_errno.h>
#include <nrf_gai_errors.h>
#include <sockets_internal.h>
#include <sys/fdtable.h>
#include <zephyr.h>

#include <stdio.h>

#include <kernel.h>

#include <random/rand32.h>
#include <net/dummy.h>
#include <net/net_pkt.h>
#include <net/net_if.h>
#include <nrf_modem_at.h>
#include <net/net_ip.h>
#include <posix/arpa/inet.h>
#include <modem/pdn.h>

#define NO_MDM_SCKT -1

K_SEM_DEFINE(mdm_socket_sem, 0, 1);

/* Work for creating modem data socket (uses system workq): */
static struct k_work_delayable mdm_socket_work;

/* Work for registering the events (uses system workq): */
static struct k_work_delayable events_work;

struct nrf91_non_offload_dev_context {
	uint8_t mac_addr[6];
	struct net_if *iface;
	bool default_pdp_active;
	int mdm_skct_id;
};

struct nrf91_non_offload_dev_context nrf91_non_offload_iface_data;

/******************************************************************************/

static void util_get_ip_addr(int cid, char *addr4, char *addr6)
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
	 * PDN type "IPV4V6": <IPv4>,<IPv6> or <IPV4> or <IPv6>
	 */
	ret = nrf_modem_at_scanf(cmd, "+CGPADDR: %*d,\"%46[.:0-9A-F]\",\"%46[:0-9A-F]\"", addr1,
				 addr2);
	if (ret <= 0) {
		return;
	}
	if (addr4 != NULL && inet_pton(AF_INET, addr1, tmp) == 1) {
		strcpy(addr4, addr1);
	} else if (addr6 != NULL && inet_pton(AF_INET6, addr1, tmp) == 1) {
		strcpy(addr6, addr1);
		return;
	}
	/* parse second IP string, IPv6 only */
	if (addr6 == NULL) {
		return;
	}
	if (ret > 1 && inet_pton(AF_INET6, addr2, tmp) == 1) {
		strcpy(addr6, addr2);
	}
}

/******************************************************************************/

static void nrf91_non_offload_pdn_event_handler(uint8_t cid, enum pdn_event event, int reason)
{
	if (cid == 0) {
		if (event == PDN_EVENT_ACTIVATED) {
			nrf91_non_offload_iface_data.default_pdp_active = true;
			/* printk("PDN_EVENT_ACTIVATED\n"); */
			k_work_schedule(&mdm_socket_work, K_SECONDS(2));
		} else if (event == PDN_EVENT_DEACTIVATED) {
			nrf91_non_offload_iface_data.default_pdp_active = false;
			k_work_schedule(&mdm_socket_work, K_NO_WAIT);
		}
	}
}

/******************************************************************************/

static void nrf91_socket_non_offload_socket_create(void)
{
	int retval;

	retval = nrf_socket(NRF_AF_PACKET, NRF_SOCK_RAW, 0);
	if (retval < 0) {
		printk("nrf_socket failed %d\n", retval);
		retval = NO_MDM_SCKT;
		k_sem_reset(&mdm_socket_sem);
	} else {
		k_sem_give(&mdm_socket_sem);
	}
	nrf91_non_offload_iface_data.mdm_skct_id = retval;
}

static void nrf91_socket_non_offload_socket_close(void)
{
	k_sem_reset(&mdm_socket_sem);
	nrf_close(nrf91_non_offload_iface_data.mdm_skct_id);
	nrf91_non_offload_iface_data.mdm_skct_id = NO_MDM_SCKT;
}

static void nrf91_non_offload_mdm_socket_worker(struct k_work *unused)
{
	ARG_UNUSED(unused);
	if (nrf91_non_offload_iface_data.default_pdp_active) {
		if (nrf91_non_offload_iface_data.mdm_skct_id == NO_MDM_SCKT) {
			char ipv4_addr[NET_IPV4_ADDR_LEN] = { 0 };
			struct sockaddr addr;
			struct net_if_addr *ifaddr;
			int len, ret;

			nrf91_socket_non_offload_socket_create();
			util_get_ip_addr(0, ipv4_addr, NULL);
			len = strlen(ipv4_addr);
			if (len == 0) {
				printk("Unable to obtain local IPv4 address");
				return;
			}
			ret = net_ipaddr_parse(ipv4_addr, len, &addr);
			if (!ret) {
				printk("Unable to parse IPv4 address");
				return;
			}
			ifaddr =
				net_if_ipv4_addr_add(nrf91_non_offload_iface_data.iface,
						     &net_sin(&addr)->sin_addr, NET_ADDR_MANUAL, 0);
			if (!ifaddr) {
				printk("Cannot add %s to interface", ipv4_addr);
				return;
			}
			/* printk("IP ADDR %s set\n", ipv4_addr); */

			/* TODO: get MTU and DNS servers and set them on zephyr stack */
		}
	} else {
		nrf91_socket_non_offload_socket_close();
	}
}

static void nrf91_non_offload_events_worker(struct k_work *unused)
{
	ARG_UNUSED(unused);

	pdn_default_callback_set(nrf91_non_offload_pdn_event_handler);
}

/******************************************************************************/

static uint8_t *fake_dev_get_mac(struct nrf91_non_offload_dev_context *ctx)
{
	if (ctx->mac_addr[2] == 0x00) {
		/* 00-00-5E-00-53-xx Documentation RFC 7042 */
		ctx->mac_addr[0] = 0x00;
		ctx->mac_addr[1] = 0x00;
		ctx->mac_addr[2] = 0x5E;
		ctx->mac_addr[3] = 0x00;
		ctx->mac_addr[4] = 0x53;
		ctx->mac_addr[5] = sys_rand32_get();
	}

	return ctx->mac_addr;
}

static void nrf91_non_offload_iface_init(struct net_if *iface)
{
	struct nrf91_non_offload_dev_context *ctx = net_if_get_device(iface)->data;
	uint8_t *mac = fake_dev_get_mac(ctx);

	ctx->iface = iface;
	ctx->default_pdp_active = false;
	ctx->mdm_skct_id = NO_MDM_SCKT;

	/* The mac address is not really used but network interface expects
	 * to find one.
	 */
	net_if_set_link_addr(iface, mac, 6, NET_LINK_ETHERNET);
}

static int nrf91_nrf_modem_lib_non_offload_init(const struct device *arg)
{
	ARG_UNUSED(arg);

	k_work_init_delayable(&mdm_socket_work, nrf91_non_offload_mdm_socket_worker);
	k_work_init_delayable(&events_work, nrf91_non_offload_events_worker);

	k_work_schedule(&events_work, K_MSEC(300));

	return 0;
}
/******************************************************************************/

#define NRF91_MODEM_DATA_UL_BUFFER_SIZE 1500
static char send_buffer[NRF91_MODEM_DATA_UL_BUFFER_SIZE];

static int nrf91_non_offload_iface_send(const struct device *dev, struct net_pkt *pkt)
{
	int ret, data_len;

	data_len = net_pkt_get_len(pkt);
	ret = net_pkt_read(pkt, send_buffer, data_len);
	if (ret < 0) {
		printk("%s: cannot read packet: %d, from pkt %p\n", __func__, ret, pkt);
	} else {
		struct nrf91_non_offload_dev_context *ctx = dev->data;

		ret = nrf_send(ctx->mdm_skct_id, send_buffer, data_len, 0);
		if (ret < 0) {
			printk("%s: send() failed: (%d), data len: %d\n", __func__, -errno,
			       data_len);
		} else if (ret != data_len) {
			printk("%s: only partially sent, only %d of original %d was sent", __func__,
			       ret, data_len);
		}
	}
	net_pkt_unref(pkt);

	return ret;
}

/******************************************************************************/

#define NRF91_MODEM_DATA_DL_POLL_TIMEOUT_MS 1000
#define NRF91_MODEM_DATA_DL_BUFFER_SIZE 1500

static char receive_buffer[NRF91_MODEM_DATA_DL_BUFFER_SIZE];

static void nrf91_modem_dl_data_thread_handler(void)
{
	struct nrf_pollfd fds[1];
	struct net_pkt *rcv_pkt;

	int ret = 0;
	int recv_data_len = 0;

	while (true) {
		if (nrf91_non_offload_iface_data.mdm_skct_id < 0) {
			/* Wait for sockets to be created */
			k_sem_take(&mdm_socket_sem, K_FOREVER);
			continue;
		}
		fds[0].fd = nrf91_non_offload_iface_data.mdm_skct_id;
		fds[0].events = NRF_POLLIN;
		fds[0].revents = 0;

		ret = nrf_poll(fds, 1, NRF91_MODEM_DATA_DL_POLL_TIMEOUT_MS);
		if (ret > 0) { /* && (fds[0].revents & POLLIN) */
			recv_data_len = nrf_recv(nrf91_non_offload_iface_data.mdm_skct_id,
						 receive_buffer, sizeof(receive_buffer), 0);
			if (recv_data_len > 0) {
				rcv_pkt = net_pkt_alloc_with_buffer(
					nrf91_non_offload_iface_data.iface, recv_data_len,
					AF_UNSPEC, 0, K_MSEC(200));
				if (!rcv_pkt) {
					printk("%s: cannot allocate rcv packet\n", (__func__));
					k_sleep(K_MSEC(100));
					continue;
				}
				if (net_pkt_write(rcv_pkt, (uint8_t *)receive_buffer,
				    recv_data_len)) {
					printk("%s: cannot write pkt %p - dropped packet\n",
					(__func__), rcv_pkt);
					net_pkt_unref(rcv_pkt);
				} else {
					ret = net_recv_data(
						nrf91_non_offload_iface_data.iface,
						rcv_pkt);
					if (ret < 0) {
						printk("%s: received packet dropped by NET stack, ret %d",
							(__func__), ret);
						net_pkt_unref(rcv_pkt);
					}
				}
			}
		} else if (ret < 0) {
			printk("%s: poll() failed %d", (__func__), ret);
		}
	}
}

#define NRF91_MODEM_DATA_DL_THREAD_STACK_SIZE 2048
#define NRF91_MODEM_DATA_RCV_THREAD_PRIORITY K_PRIO_COOP(10) /* -6 */

K_THREAD_DEFINE(nrf91_modem_dl_data_thread, NRF91_MODEM_DATA_DL_THREAD_STACK_SIZE,
		nrf91_modem_dl_data_thread_handler, NULL, NULL, NULL,
		NRF91_MODEM_DATA_RCV_THREAD_PRIORITY, 0, 0);

/******************************************************************************/

static struct dummy_api nrf91_non_offload_if_api = {
	.iface_api.init = nrf91_non_offload_iface_init,
	.send = nrf91_non_offload_iface_send,
};

/* No L2 */
#define NRF91_L2_LAYER DUMMY_L2
#define NRF91_L2_CTX_TYPE NET_L2_GET_CTX_TYPE(DUMMY_L2)
#define NRF91_MTU 1500

NET_DEVICE_INIT(nrf91_non_offload, "nrf91_nrf_non_offload", nrf91_nrf_modem_lib_non_offload_init,
		NULL, &nrf91_non_offload_iface_data, NULL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		&nrf91_non_offload_if_api, NRF91_L2_LAYER, NRF91_L2_CTX_TYPE, NRF91_MTU);
