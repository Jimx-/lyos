#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

/* #define NETIF_DEBUG LWIP_DBG_ON */
/* #define IP_DEBUG    LWIP_DBG_ON */
/* #define ETHARP_DEBUG LWIP_DBG_ON */
/* #define TCP_DEBUG    LWIP_DBG_ON */

#define NO_SYS               1
#define LWIP_TIMERS          1
#define SYS_LIGHTWEIGHT_PROT 0

#define MEM_LIBC_MALLOC 1

#define MEM_ALIGNMENT 4

#define MEMP_MEM_MALLOC 1

#define MEMP_OVERFLOW_CHECK 0
#define MEMP_SANITY_CHECK   0

#define MEM_USE_POOLS         0
#define MEMP_USE_CUSTOM_POOLS 0

#define LWIP_ARP 1

#define ARP_TABLE_SIZE 16

#define ARP_QUEUEING 1

#define ETHARP_SUPPORT_VLAN 0

#define ETH_PAD_SIZE 0

#define ETHARP_SUPPORT_STATIC_ENTRIES 1

#define ETHARP_TABLE_MATCH_NETIF 1

#define LWIP_IPV4 1

#define IP_FORWARD 1

#define IP_FRAG 1

#define MEMP_NUM_FRAG_PBUF 256

#define IP_REASSEMBLY 1

#define MEMP_NUM_REASSDATA 16

#define IP_REASS_MAX_PBUFS 129

#define IP_SOF_BROADCAST      1
#define IP_SOF_BROADCAST_RECV 0

#define LWIP_RANDOMIZE_INITIAL_LOCAL_PORTS 1

#define LWIP_ICMP 1

#define LWIP_MULTICAST_PING 1

#define LWIP_RAW 1

#define MEMP_NUM_RAW_PCB 16

#define LWIP_DHCP 0

#define LWIP_AUTOIP 0

#define LWIP_MULTICAST_TX_OPTIONS 1

#define LWIP_IGMP 1

#define NR_IPV4_MCAST_GROUP 64

#define MEMP_NUM_IGMP_GROUP (16 + NR_IPV4_MCAST_GROUP)

#define LWIP_DNS 0

#define LWIP_UDP 1

#define MEMP_NUM_UDP_PCB 256

#define LWIP_TCP 1

#define TCP_MSS 1460

#define TCP_WND 16384

#define TCP_SND_BUF (11 * TCP_MSS)

#define TCP_LISTEN_BACKLOG 1

#define TCP_OVERSIZE 0

#define LWIP_EVENT_API    0
#define LWIP_CALLBACK_API 1

#define LWIP_NETIF_STATUS_CALLBACK 1

#define LWIP_NETIF_HWADDRHINT 1

#define LWIP_NETIF_LOOPBACK 0

#define LWIP_NETCONN 0
#define LWIP_SOCKET  0

#define LWIP_TCP_KEEPALIVE 1

#define SO_REUSE         1
#define SO_REUSE_RXTOALL 1

#define LWIP_STATS 0

#define LWIP_CHECKSUM_CTRL_PER_NETIF 1
#define CHECKSUM_GEN_IP              1
#define CHECKSUM_GEN_UDP             1
#define CHECKSUM_GEN_TCP             1
#define CHECKSUM_GEN_ICMP            1
#define CHECKSUM_GEN_ICMP6           1
#define CHECKSUM_CHECK_IP            1
#define CHECKSUM_CHECK_UDP           1
#define CHECKSUM_CHECK_TCP           1
#define CHECKSUM_CHECK_ICMP          1
#define CHECKSUM_CHECK_ICMP6         1

#define LWIP_CHECKSUM_ON_COPY 0

#define LWIP_CHKSUM_ALGORITHM 3

#define LWIP_IPV6 1

#define LWIP_IPV6_SCOPES       1
#define LWIP_IPV6_SCOPES_DEBUG 1

#define LWIP_IPV6_NUM_ADDRESSES 4

#define LWIP_IPV6_FORWARD 1

#define LWIP_IPV6_FRAG  1
#define LWIP_IPV6_REASS 1

#define LWIP_IPV6_SEND_ROUTER_SOLICIT 0

#define LWIP_IPV6_AUTOCONFIG 0

#define LWIP_IPV6_ADDRESS_LIFETIMES 1

#define LWIP_IPV6_DUP_DETECT_ATTEMPTS 3

#define LWIP_ICMP6 1

#define LWIP_IPV6_MLD 1

#define LWIP_ND6_QUEUEING 1

#define LWIP_ND6_NUM_NEIGHBORS    16
#define LWIP_ND6_NUM_DESTINATIONS 16

#define LWIP_ND6_NUM_PREFIXES 1
#define LWIP_ND6_NUM_ROUTERS  1

#endif /* !LWIP_LWIPOPTS_H */
