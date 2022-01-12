#ifndef _NET_ROUTE_H
#define _NET_ROUTE_H 1

/* This structure gets passed by the SIOCADDRT and SIOCDELRT calls. */
struct rtentry {
    unsigned long int rt_pad1;
    struct sockaddr rt_dst;     /* Target address.  */
    struct sockaddr rt_gateway; /* Gateway addr (RTF_GATEWAY).  */
    struct sockaddr rt_genmask; /* Target network mask (IP).  */
    unsigned short int rt_flags;
    short int rt_pad2;
    unsigned long int rt_pad3;
    unsigned char rt_tos;
    unsigned char rt_class;
#if __WORDSIZE == 64
    short int rt_pad4[3];
#else
    short int rt_pad4;
#endif
    short int rt_metric;         /* +1 for binary compatibility!  */
    char* rt_dev;                /* Forcing the device at add.  */
    unsigned long int rt_mtu;    /* Per route MTU/Window.  */
    unsigned long int rt_window; /* Window clamping.  */
    unsigned short int rt_irtt;  /* Initial RTT.  */
};
/* Compatibility hack.  */
#define rt_mss rt_mtu

#endif
