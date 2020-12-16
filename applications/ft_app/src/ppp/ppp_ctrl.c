#include <zephyr.h>
#include <stdio.h>
#include <assert.h>

#include <net/ppp.h>

#include <net/net_ip.h>
#include <net/net_if.h>

#include <net/net_event.h>
#include <net/net_mgmt.h>

#include <posix/unistd.h>
#include <posix/netdb.h>

#include <posix/poll.h>
#include <posix/sys/socket.h>
#include <shell/shell.h>

#include "ltelc_api.h"

#include "ppp_ctrl.h"

#if defined (CONFIG_FTA_PPP)
#define PPP_CTRL_UPLINK_WORKER 1

#define RAW_SCKT_FD_NONE -666

struct net_if *ppp_iface_global;
static const struct shell* shell_global;
static int socket_fd = RAW_SCKT_FD_NONE;

typedef enum net_verdict (*ppp_l2_callback_t)(struct net_if *iface,
					      struct net_pkt *pkt);

void net_core_register_pkt_cb(ppp_l2_callback_t cb); /* found in net_core.c */

static void ppp_ctrl_send_to_modem(struct net_pkt *pkt)
{
	static uint8_t buf_tx[CONFIG_NET_PPP_MTU_MRU];
	int ret = 0;
	int data_len = net_pkt_remaining_data(pkt);

	ret = net_pkt_read(pkt, buf_tx, data_len);
	if (ret < 0) {
		shell_error(shell_global, "cannot read packet: %d, from pkt %p", ret, pkt);
	} else {	
		ret = send(socket_fd, buf_tx, data_len, 0);
		if (ret <= 0) {
			shell_error(shell_global, "send() failed: (%d), data len: %d\n", ret, data_len);
		}
	}
	net_pkt_unref(pkt);
}
/* ********************************************************************/
#ifdef PPP_CTRL_UPLINK_WORKER

#define UPLINK_WORKQUEUE_STACK_SIZE 2048
#define UPLINK_WORKQUEUE_PRIORITY 5
K_THREAD_STACK_DEFINE(uplink_stack_area, UPLINK_WORKQUEUE_STACK_SIZE);
#if 0
//re-using work in pkt
struct uplink_info {
	struct k_work work;
	struct net_pkt *pkt;
};
#endif
//struct k_work uplink_work;
struct k_work_q uplink_work_q;

static void ppp_ctrl_process_ppp_rx_packet(struct k_work *item)
{
	struct net_pkt *pkt;
	pkt = CONTAINER_OF(item, struct net_pkt, work);

	ppp_ctrl_send_to_modem(pkt);
}
#endif

static enum net_verdict ppp_ctrl_data_recv(struct net_if *iface, struct net_pkt *pkt)
{
	//TODO?
	//iface not needed as parameter? set in pkt and can be get:	iface = net_pkt_iface(pkt);
	if (!pkt->buffer) {
		shell_info(shell_global,"MoSH: ppp_ctrl_data_recv: No data to recv!");
		goto drop;
	}
	if (ppp_iface_global != iface) {
		/* Tai globalin sijaan:
		net_if_l2(net_pkt_iface(pkt)) == &NET_L2_GET_NAME(PPP)) */
		shell_error(shell_global, "MoSH: ppp_ctrl_data_recv: not for ppp iface\n");
		return NET_CONTINUE;
	}
	if (socket_fd == RAW_SCKT_FD_NONE) {
		shell_error(shell_global, "MoSH: ppp_ctrl_data_recv: no socket to modem\n");
		return NET_CONTINUE;
	}

	char type = (NET_IPV6_HDR(pkt)->vtc & 0xf0);
	if (type != 0x40) {
		shell_error(shell_global, "MoSH: ppp_ctrl_data_recv: not IPv4 data\n");
		goto drop;
	}
#ifdef PPP_CTRL_UPLINK_WORKER
	k_work_init(net_pkt_work(pkt), ppp_ctrl_process_ppp_rx_packet);
	k_work_submit_to_queue(&uplink_work_q, net_pkt_work(pkt));
#else
	int ret = 0;
	static uint8_t buf_tx[1500];//TODO: to be CONFIG_NET_PPP_MTU_MRU
	int data_len = net_pkt_remaining_data(pkt);

	ret = net_pkt_read(pkt, buf_tx, data_len);
	if (ret < 0) {
		shell_error(shell_global, "cannot read packet: %d, from pkt %p", ret, pkt);
		goto drop;
	}
	
	ret = send(socket_fd, buf_tx, data_len, 0);
	if (ret <= 0) {
		shell_error(shell_global, "send() failed: (%d), data len: %d\n", ret, data_len);
		goto drop;
	}
	net_pkt_unref(pkt);
#endif
	return NET_OK;

drop:
	return NET_DROP;
}
static void ppp_shell_set_ppp_carrier_on()
{
	const struct device *ppp_dev = device_get_binding(CONFIG_NET_PPP_DRV_NAME);
	const struct ppp_api *api;
	/* olisko parempi:
		iface = net_if_get_first_by_type(&NET_L2_GET_NAME(PPP));
	*/

	if (!ppp_dev) {
		printf("Cannot find PPP %s!", "device");
		return;
	}
	shell_print(shell_global, "Starting PPP");


	api = (const struct ppp_api *)ppp_dev->api;
	api->start(ppp_dev);
}
/* *******************************************/
static struct net_mgmt_event_callback mgmt_ip_cb;
static struct net_mgmt_event_callback mgmt_ppp_cb;

static void ppp_shell_net_event_handler(struct net_mgmt_event_callback *cb,
			  uint32_t mgmt_event, struct net_if *iface)
{
	printf("\nppp_shell_net_event_handler %d\n", mgmt_event);

	if ((mgmt_event & (NET_EVENT_PPP_CARRIER_ON
			   | NET_EVENT_PPP_CARRIER_OFF | NET_EVENT_IPV4_ADDR_DEL)) != mgmt_event) {
		return;
	}

	if (mgmt_event == NET_EVENT_PPP_CARRIER_ON) {
		printf("PPP carrier ON\n");
		return;
	}

	if (mgmt_event == NET_EVENT_PPP_CARRIER_OFF) {
		printf("PPP carrier OFF\n");
		if (socket_fd != RAW_SCKT_FD_NONE) {
			(void)close(socket_fd);
			socket_fd = RAW_SCKT_FD_NONE;
		}
		return;
	}

	if (mgmt_event == NET_EVENT_IPV4_ADDR_DEL) {
		printf("NET_EVENT_IPV4_ADDR_DEL: somebody removed the ip from PPP interface\n");
		return;
	}
}

static void ppp_shell_net_events_subscribe()
{
	net_mgmt_init_event_callback(&mgmt_ip_cb, ppp_shell_net_event_handler,
				     NET_EVENT_IPV4_ADDR_DEL);
	net_mgmt_add_event_callback(&mgmt_ip_cb);

	net_mgmt_init_event_callback(&mgmt_ppp_cb, ppp_shell_net_event_handler,
				     (NET_EVENT_PPP_CARRIER_ON | NET_EVENT_PPP_CARRIER_OFF));
	net_mgmt_add_event_callback(&mgmt_ppp_cb);
}

/* ****************************************/

/* *******************************************/
void ppp_ctrl_init()
{
	socket_fd = RAW_SCKT_FD_NONE;
	//init iface
	//net_if_flag_set(ictx.iface, NET_IF_NO_AUTO_START);
#ifdef PPP_CTRL_UPLINK_WORKER
	k_work_q_start(&uplink_work_q, uplink_stack_area,
		       K_THREAD_STACK_SIZEOF(uplink_stack_area),
		       UPLINK_WORKQUEUE_PRIORITY);
	k_thread_name_set(&uplink_work_q.thread, "mosh_uplink_work_q");
#endif
}

int ppp_ctrl_start(const struct shell *shell) {
	struct ppp_context *ctx;
	struct net_if *iface;
	#if defined(CONFIG_NET_IPV4)
	struct net_if_addr *ifaddr;
	struct net_if_ipv4 *ipv4;
#endif
	int idx = 0; //TODO: find PPP if according to name?
	pdp_context_info_t* pdp_context_info;

	shell_global = shell;

	ppp_shell_net_events_subscribe();

	ctx = net_ppp_context_get(idx);
	if (!ctx) {
		shell_error(shell, "PPP context not found.\n");
		goto return_error;
	}
	pdp_context_info = ltelc_api_get_pdp_context_info_by_pdn_cid(0);//TODO: multi context support
	if (pdp_context_info == NULL) {
		shell_error(shell, "PPP context not found.\n");
		goto return_error;
	}

	iface = ctx->iface;
	ppp_iface_global = iface;
	net_if_flag_set(iface, NET_IF_NO_AUTO_START);
#if defined(CONFIG_NET_IPV4)
	if (net_if_config_ipv4_get(iface, &ipv4) < 0) {
		shell_info(shell, "no ip address\n");
	}
	else {
		bool removed = false;
		/* remove the current IPv4 addr before adding a new one.*/
		removed = net_if_ipv4_addr_rm(iface, &ctx->ipcp.my_options.address);
		shell_info(shell, "removed %d \n", removed);
	}
#endif

	/* Couldn't find the way to set these for PPP in another way: TODO api to PPP for raw mode?*/
	memcpy(&(ctx->ipcp.my_options.address), &(pdp_context_info->sin4.sin_addr), sizeof(ctx->ipcp.my_options.address));
    memcpy(&ctx->ipcp.my_options.dns1_address, &pdp_context_info->dns_addr4_primary, sizeof(ctx->ipcp.my_options.dns1_address));
    memcpy(&ctx->ipcp.my_options.dns2_address, &pdp_context_info->dns_addr4_secondary, sizeof(ctx->ipcp.my_options.dns2_address));

	free(pdp_context_info);
	
	/* Set the IP to netif: */
#if defined(CONFIG_NET_IPV4)
	shell_print(shell, "calling net_if_ipv4_addr_add...\n");
	ifaddr = net_if_ipv4_addr_add(iface, &my_ipv4_addr1, NET_ADDR_DHCP, 0);//ei vaikutusta ppp contextiin, ylläoleva memcpy tekee sen
	if (!ifaddr) {
		shell_error(shell, "Cannot add IPv4 address\n");
		goto return_error;
	}
	shell_print(shell, "calling ppp_shell_set_ppp_carrier_on...\n");
#endif

	socket_fd = socket(AF_PACKET, SOCK_RAW, 0);
	if (socket_fd < 0) {
		shell_error(shell, "socket creation failed: (%d)!!!!\n", -errno);
		goto return_error;
	}
	else {
		shell_info(shell, "socket %d created for modem data", socket_fd);
	}

	ppp_shell_set_ppp_carrier_on();

	net_core_register_pkt_cb(ppp_ctrl_data_recv);
	return 0;

return_error:
	return -1;
}

void ppp_shell_set_ppp_carrier_off()
{
	struct ppp_context *ctx;
	int idx = 0; //TODO: find PPP ifaccording to name?

	printf("ppp_shell_set_ppp_carrier_off\n");
		
	ctx = net_ppp_context_get(idx);
	if (!ctx && !ctx->iface)
		return;

	const struct device *ppp_dev = net_if_get_device(ctx->iface);
	const struct ppp_api *api;
		
	api = (const struct ppp_api *)ppp_dev->api;
	api->stop(ppp_dev);
}


/* *************************************************************************************/
#define PPP_RECEIVE_STACK_SIZE 2048
#define PPP_RECEIVE_PRIORITY 5
#define SOCK_POLL_TIMEOUT_MS 1000 // Milliseconds
#define SOCK_RECEIVE_BUFFER_SIZE 1500 //TODO
static char receive_buffer[SOCK_RECEIVE_BUFFER_SIZE];

static void ppp_ctrl_modem_data_receive_handler()
{
	struct pollfd fds[1];
	struct net_if *iface;
	struct net_pkt *pkt;

	int ret = 0;
	int recv_data_len = 0;

	iface = net_if_get_first_by_type(&NET_L2_GET_NAME(PPP));
	assert(iface != NULL);
	
	while (true) {
		if (socket_fd < 0) {
			/* No sockets in use, so no use calling poll() */
			k_sleep(K_MSEC(SOCK_POLL_TIMEOUT_MS));
			continue;
		}
		else {
			fds[0].fd = socket_fd;
			fds[0].events = POLLIN;
			fds[0].revents = 0;

			ret = poll(fds, 1, SOCK_POLL_TIMEOUT_MS);
			if (ret > 0) {
				recv_data_len = recv(socket_fd, receive_buffer, SOCK_RECEIVE_BUFFER_SIZE, 0);
				if (recv_data_len > 0) {
					//shell_info(shell_global, "ppp_ctrl_modem_data_receive_handler: data received from modem, len %d", recv_data_len);


					pkt = net_pkt_alloc_with_buffer(iface, recv_data_len, AF_UNSPEC, 0, K_NO_WAIT);
					if (!pkt) {
						shell_error(shell_global, "ppp_ctrl_modem_data_receive_handler: no buf available - dropped packet");
					} else {
						//memcpy(pkt->buffer->data, receive_buffer, recv_data_len);
						//net_buf_add(pkt->buffer, recv_data_len);
						if (net_pkt_write(pkt, (uint8_t *)receive_buffer, recv_data_len)) {
							shell_error(shell_global,"ppp_ctrl_modem_data_receive_handler: cannot write pkt %p - dropped packet", pkt);
							net_pkt_unref(pkt);
						} else {
							char type = (NET_IPV6_HDR(pkt)->vtc & 0xf0);
							
							switch (type) {
								case 0x60:
									net_pkt_set_family(pkt, AF_INET6);
									break;
								case 0x40:					
									net_pkt_set_family(pkt, AF_INET);
									break;
							}

							if (net_send_data(pkt) < 0) {
								shell_error(shell_global,"ppp_ctrl_modem_data_receive_handler: cannot send data pkt %p - dropped packet", pkt);
								net_pkt_unref(pkt);
							}						
						}
					}
				} else
				{
					shell_error(shell_global,"ppp_ctrl_modem_data_receive_handler: recv() failed %d", recv_data_len);
				}
			} else if (ret < 0) {
				shell_error(shell_global,"ppp_ctrl_modem_data_receive_handler: poll() failed %d", ret);
			}
		}

	}

}

K_THREAD_DEFINE(ppp_ctrl_modem_data_receive_thread, PPP_RECEIVE_STACK_SIZE,
                ppp_ctrl_modem_data_receive_handler, NULL, NULL, NULL,
                PPP_RECEIVE_PRIORITY, 0, 0);

#endif /* CONFIG_FTA_PPP */