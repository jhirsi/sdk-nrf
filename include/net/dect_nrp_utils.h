#ifndef DECT_NRP_UTILS_H
#define DECT_NRP_UTILS_H

#include <zephyr/net/net_ip.h>

uint32_t dect_nrp_utils_dst_long_rd_id_get_from_dst_sock_ll_addr(struct sockaddr_ll *dst_addr);

#include <zephyr/net/net_pkt.h>
uint32_t dect_nrp_utils_dst_long_rd_id_get_from_pkt_dst_addr(struct net_pkt *pkt);

bool dect_nrp_utils_net_linkaddr_set_from_long_rd_id(
	struct net_linkaddr *lladdr, uint32_t long_rd_id);
bool dect_nrp_utils_net_linkaddr_reset_from_long_rd_id(
	struct net_linkaddr *lladdr, uint32_t long_rd_id);

void dect_nrp_utils_net_ipv6_addr_create_iid(struct in6_addr *addr, struct net_linkaddr *lladdr);
uint32_t dect_nrp_utils_long_rd_id_from_ipv6_addr(struct in6_addr *addr);

bool dect_nrp_utils_net_ipv6_addr_create_from_sink_and_long_rd_id(
	struct in6_addr prefix_64, uint32_t sink_rd_id, uint32_t own_rd_id, struct in6_addr *addr);

uint8_t dect_nrp_utils_dbm_to_phy_tx_power(int8_t pwr_dBm);
int8_t dect_nrp_utils_phy_tx_power_to_dbm(uint8_t phy_power);

bool dect_nrp_utils_32bit_network_id_validate(uint32_t network_id);

#endif /* DECT_NRP_UTILS_H */
