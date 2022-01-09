#ifndef _INET_IPSOCK_H_
#define _INET_IPSOCK_H_

struct ipsock {
    struct sock sock;
    unsigned int flags;
    size_t sndbuf;
    size_t rcvbuf;
};

#define IPF_IPV6 0x1

#define TCPF_CONNECTING 0x1000
#define TCPF_SENT_FIN   0x2000
#define TCPF_RCVD_FIN   0x4000
#define TCPF_FULL       0x8000
#define TCPF_OOM        0x10000

#define ipsock_get_sock(ip)         (&(ip)->sock)
#define ipsock_get_sndbuf(ip)       ((ip)->sndbuf)
#define ipsock_get_rcvbuf(ip)       ((ip)->rcvbuf)
#define ipsock_get_flags(ip)        ((ip)->flags)
#define ipsock_get_flag(ip, flag)   ((ip)->flags & (flag))
#define ipsock_set_flag(ip, flag)   ((ip)->flags |= (flag))
#define ipsock_clear_flag(ip, flag) ((ip)->flags &= ~(flag))

int ipsock_socket(struct ipsock* ip, int domain, size_t sndbuf, size_t rcvbuf,
                  struct sock** sockp);

void ipsock_clone(struct ipsock* ip, struct ipsock* newip, sockid_t newid);

int ipsock_get_src_addr(struct ipsock* ip, const struct sockaddr* addr,
                        socklen_t addr_len, endpoint_t user_endpt,
                        ip_addr_t* local_ip, uint16_t local_port,
                        int allow_mcast, ip_addr_t* src_addr,
                        uint16_t* src_port);
int ipsock_get_dst_addr(struct ipsock* ip, const struct sockaddr* addr,
                        socklen_t addr_len, ip_addr_t* local_ip,
                        ip_addr_t* dst_addr, uint16_t* dst_port);
void ipsock_set_addr(struct ipsock* ip, struct sockaddr* addr,
                     socklen_t* addr_len, ip_addr_t* ipaddr, uint16_t port);

#endif
