#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#include <sys/cdefs.h>
#include <sys/socket.h>
#include <stdint.h>

__BEGIN_DECLS

typedef uint32_t in_addr_t;
struct in_addr {
    in_addr_t s_addr;
};

/* Standard well-defined IP protocols.  */
enum {
    IPPROTO_IP = 0, /* Dummy protocol for TCP.  */
#define IPPROTO_IP IPPROTO_IP
    IPPROTO_ICMP = 1, /* Internet Control Message Protocol.  */
#define IPPROTO_ICMP IPPROTO_ICMP
    IPPROTO_IGMP = 2, /* Internet Group Management Protocol. */
#define IPPROTO_IGMP IPPROTO_IGMP
    IPPROTO_IPIP = 4, /* IPIP tunnels (older KA9Q tunnels use 94).  */
#define IPPROTO_IPIP IPPROTO_IPIP
    IPPROTO_TCP = 6, /* Transmission Control Protocol.  */
#define IPPROTO_TCP IPPROTO_TCP
    IPPROTO_EGP = 8, /* Exterior Gateway Protocol.  */
#define IPPROTO_EGP IPPROTO_EGP
    IPPROTO_PUP = 12, /* PUP protocol.  */
#define IPPROTO_PUP IPPROTO_PUP
    IPPROTO_UDP = 17, /* User Datagram Protocol.  */
#define IPPROTO_UDP IPPROTO_UDP
    IPPROTO_IDP = 22, /* XNS IDP protocol.  */
#define IPPROTO_IDP IPPROTO_IDP
    IPPROTO_TP = 29, /* SO Transport Protocol Class 4.  */
#define IPPROTO_TP IPPROTO_TP
    IPPROTO_DCCP = 33, /* Datagram Congestion Control Protocol.  */
#define IPPROTO_DCCP IPPROTO_DCCP
    IPPROTO_IPV6 = 41, /* IPv6 header.  */
#define IPPROTO_IPV6 IPPROTO_IPV6
    IPPROTO_RSVP = 46, /* Reservation Protocol.  */
#define IPPROTO_RSVP IPPROTO_RSVP
    IPPROTO_GRE = 47, /* General Routing Encapsulation.  */
#define IPPROTO_GRE IPPROTO_GRE
    IPPROTO_ESP = 50, /* encapsulating security payload.  */
#define IPPROTO_ESP IPPROTO_ESP
    IPPROTO_AH = 51, /* authentication header.  */
#define IPPROTO_AH IPPROTO_AH
    IPPROTO_MTP = 92, /* Multicast Transport Protocol.  */
#define IPPROTO_MTP IPPROTO_MTP
    IPPROTO_BEETPH = 94, /* IP option pseudo header for BEET.  */
#define IPPROTO_BEETPH IPPROTO_BEETPH
    IPPROTO_ENCAP = 98, /* Encapsulation Header.  */
#define IPPROTO_ENCAP IPPROTO_ENCAP
    IPPROTO_PIM = 103, /* Protocol Independent Multicast.  */
#define IPPROTO_PIM IPPROTO_PIM
    IPPROTO_COMP = 108, /* Compression Header Protocol.  */
#define IPPROTO_COMP IPPROTO_COMP
    IPPROTO_SCTP = 132, /* Stream Control Transmission Protocol.  */
#define IPPROTO_SCTP IPPROTO_SCTP
    IPPROTO_UDPLITE = 136, /* UDP-Lite protocol.  */
#define IPPROTO_UDPLITE IPPROTO_UDPLITE
    IPPROTO_MPLS = 137, /* MPLS in IP.  */
#define IPPROTO_MPLS IPPROTO_MPLS
    IPPROTO_ETHERNET = 143, /* Ethernet-within-IPv6 Encapsulation.  */
#define IPPROTO_ETHERNET IPPROTO_ETHERNET
    IPPROTO_RAW = 255, /* Raw IP packets.  */
#define IPPROTO_RAW IPPROTO_RAW
    IPPROTO_MPTCP = 262, /* Multipath TCP connection.  */
#define IPPROTO_MPTCP IPPROTO_MPTCP
    IPPROTO_MAX
};

typedef uint16_t in_port_t;

/* Standard well-known ports.  */
enum {
    IPPORT_ECHO = 7,        /* Echo service.  */
    IPPORT_DISCARD = 9,     /* Discard transmissions service.  */
    IPPORT_SYSTAT = 11,     /* System status service.  */
    IPPORT_DAYTIME = 13,    /* Time of day service.  */
    IPPORT_NETSTAT = 15,    /* Network status service.  */
    IPPORT_FTP = 21,        /* File Transfer Protocol.  */
    IPPORT_TELNET = 23,     /* Telnet protocol.  */
    IPPORT_SMTP = 25,       /* Simple Mail Transfer Protocol.  */
    IPPORT_TIMESERVER = 37, /* Timeserver service.  */
    IPPORT_NAMESERVER = 42, /* Domain Name Service.  */
    IPPORT_WHOIS = 43,      /* Internet Whois service.  */
    IPPORT_MTP = 57,

    IPPORT_TFTP = 69, /* Trivial File Transfer Protocol.  */
    IPPORT_RJE = 77,
    IPPORT_FINGER = 79, /* Finger service.  */
    IPPORT_TTYLINK = 87,
    IPPORT_SUPDUP = 95, /* SUPDUP protocol.  */

    IPPORT_EXECSERVER = 512,  /* execd service.  */
    IPPORT_LOGINSERVER = 513, /* rlogind service.  */
    IPPORT_CMDSERVER = 514,
    IPPORT_EFSSERVER = 520,

    /* UDP ports.  */
    IPPORT_BIFFUDP = 512,
    IPPORT_WHOSERVER = 513,
    IPPORT_ROUTESERVER = 520,

    /* Ports less than this value are reserved for privileged processes.  */
    IPPORT_RESERVED = 1024,

    /* Ports greater this value are reserved for (non-privileged) servers.  */
    IPPORT_USERRESERVED = 5000
};

/* Structure describing an Internet socket address.  */
struct sockaddr_in {
    __SOCKADDR_COMMON(sin_);
    in_port_t sin_port;      /* Port number.  */
    struct in_addr sin_addr; /* Internet address.  */

    /* Pad to size of `struct sockaddr'.  */
    unsigned char sin_zero[sizeof(struct sockaddr) - __SOCKADDR_COMMON_SIZE -
                           sizeof(in_port_t) - sizeof(struct in_addr)];
};

#define INET_ADDRSTRLEN  16
#define INET6_ADDRSTRLEN 46

uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);

#define IN6_IS_ADDR_UNSPECIFIED(a)                \
    ({                                            \
        uint32_t* _a = (uint32_t*)((a)->s6_addr); \
        !_a[0] && !_a[1] && !_a[2] && !_a[3];     \
    })
#define IN6_IS_ADDR_LOOPBACK(a)                               \
    ({                                                        \
        uint32_t* _a = (uint32_t*)((a)->s6_addr);             \
        !_a[0] && !_a[1] && !_a[2] && _a[3] == htonl(0x0001); \
    })
#define IN6_IS_ADDR_MULTICAST(a) (((const uint8_t*)(a))[0] == 0xff)
#define IN6_IS_ADDR_LINKLOCAL(a)                        \
    ({                                                  \
        uint32_t* _a = (uint32_t*)((a)->s6_addr);       \
        _a[0] & htonl(0xffc00000) == htonl(0xfe800000); \
    })
#define IN6_IS_ADDR_SITELOCAL(a)                        \
    ({                                                  \
        uint32_t* _a = (uint32_t*)((a)->s6_addr);       \
        _a[0] & htonl(0xffc00000) == htonl(0xfec00000); \
    })
#define IN6_IS_ADDR_V4MAPPED(a)                     \
    ({                                              \
        uint32_t* _a = (uint32_t*)((a)->s6_addr);   \
        !_a[0] && !_a[1] && _a[2] == htonl(0xffff); \
    })
#define __ARE_4_BYTE_EQUAL(a, b)                                 \
    ((a)[0] == (b)[0] && (a)[1] == (b)[1] && (a)[2] == (b)[2] && \
     (a)[3] == (b)[3] && (a)[4] == (b)[4])
#define IN6_ARE_ADDR_EQUAL(a, b) \
    __ARE_4_BYTE_EQUAL((const uint32_t*)(a), (const uint32_t*)(b))

#define IN6_IS_ADDR_V4COMPAT 7
#define IN6_IS_ADDR_MC_NODELOCAL(a)                  \
    ({                                               \
        (IN6_IS_ADDR_MULTICAST(a) &&                 \
         ((((const uint8_t*)(a))[1] & 0xf) == 0x1)); \
    })
#define IN6_IS_ADDR_MC_LINKLOCAL(a)                  \
    ({                                               \
        (IN6_IS_ADDR_MULTICAST(a) &&                 \
         ((((const uint8_t*)(a))[1] & 0xf) == 0x2)); \
    })
#define IN6_IS_ADDR_MC_SITELOCAL(a)                  \
    ({                                               \
        (IN6_IS_ADDR_MULTICAST(a) &&                 \
         ((((const uint8_t*)(a))[1] & 0xf) == 0x5)); \
    })
#define IN6_IS_ADDR_MC_ORGLOCAL(a)                   \
    ({                                               \
        (IN6_IS_ADDR_MULTICAST(a) &&                 \
         ((((const uint8_t*)(a))[1] & 0xf) == 0x8)); \
    })
#define IN6_IS_ADDR_MC_GLOBAL(a)                     \
    ({                                               \
        (IN6_IS_ADDR_MULTICAST(a) &&                 \
         ((((const uint8_t*)(a))[1] & 0xf) == 0xe)); \
    })

#define IN_CLASSD(a)    ((((in_addr_t)(a)) & 0xf0000000) == 0xe0000000)
#define IN_MULTICAST(a) IN_CLASSD(a)

#define IN_CLASSA_NET 0xff000000
#define IN_CLASSB_NET 0xffff0000
#define IN_CLASSC_NET 0xffffff00

__END_DECLS

#endif // _NETINET_IN_H
