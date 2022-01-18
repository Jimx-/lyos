#ifndef _NETDB_H
#define _NETDB_H

#include <stdint.h>
#include <netinet/in.h>
#include <bits/socket.h>

#define AI_PASSIVE     0x01
#define AI_CANONNAME   0x02
#define AI_NUMERICHOST 0x04
#define AI_V4MAPPED    0x08
#define AI_ALL         0x10
#define AI_ADDRCONFIG  0x20
#define AI_NUMERICSERV 0x400

#define NI_NOFQDN       0x01
#define NI_NUMERICHOST  0x02
#define NI_NAMEREQD     0x04
#define NI_NUMERICSCOPE 0x08
#define NI_DGRAM        0x10

#define NI_NUMERICSERV 2
#define NI_MAXSERV     32

#define NI_MAXHOST 1025

/* Error values for `getaddrinfo' function.  */
#define EAI_BADFLAGS    -1   /* Invalid value for `ai_flags' field.  */
#define EAI_NONAME      -2   /* NAME or SERVICE is unknown.  */
#define EAI_AGAIN       -3   /* Temporary failure in name resolution.  */
#define EAI_FAIL        -4   /* Non-recoverable failure in name res.  */
#define EAI_FAMILY      -6   /* `ai_family' not supported.  */
#define EAI_SOCKTYPE    -7   /* `ai_socktype' not supported.  */
#define EAI_SERVICE     -8   /* SERVICE not supported for `ai_socktype'.  */
#define EAI_MEMORY      -10  /* Memory allocation failure.  */
#define EAI_SYSTEM      -11  /* System error returned in `errno'.  */
#define EAI_OVERFLOW    -12  /* Argument buffer overflow.  */
#define EAI_NODATA      -5   /* No address associated with NAME.  */
#define EAI_ADDRFAMILY  -9   /* Address family for NAME not supported.  */
#define EAI_INPROGRESS  -100 /* Processing request in progress.  */
#define EAI_CANCELED    -101 /* Request canceled.  */
#define EAI_NOTCANCELED -102 /* Request not canceled.  */
#define EAI_ALLDONE     -103 /* All requests done.  */
#define EAI_INTR        -104 /* Interrupted by a signal.  */
#define EAI_IDN_ENCODE  -105 /* IDN encoding failed.  */

#define HOST_NOT_FOUND 1
#define TRY_AGAIN      2
#define NO_RECOVERY    3
#define NO_DATA        4
#define NO_ADDRESS     NO_DATA

#define IPPORT_RESERVED 1024

#define _PATH_SERVICES "/etc/services"

__BEGIN_DECLS

int* __h_errno_location(void);
#define h_errno (*__h_errno_location())

struct hostent {
    char* h_name;
    char** h_aliases;
    int h_addrtype;
    int h_length;
    char** h_addr_list;
};

#define h_addr h_addr_list[0] // Required by some programs

struct netent {
    char* n_name;
    char** n_aliases;
    int n_addrtype;
    uint32_t n_net;
};

struct protoent {
    char* p_name;
    char** p_aliases;
    int p_proto;
};

struct servent {
    char* s_name;
    char** s_aliases;
    int s_port;
    char* s_proto;
};

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr* ai_addr;
    char* ai_canonname;
    struct addrinfo* ai_next;
};

void endhostent(void);
void endnetent(void);
void endprotoent(void);
void endservent(void);
void freeaddrinfo(struct addrinfo*);
const char* gai_strerror(int);
int getaddrinfo(const char* __restrict, const char* __restrict,
                const struct addrinfo* __restrict,
                struct addrinfo** __restrict);
struct hostent* gethostent(void);
struct hostent* gethostbyname(const char*);
struct hostent* gethostbyaddr(const void*, socklen_t, int);
int getnameinfo(const struct sockaddr* __restrict, socklen_t, char* __restrict,
                socklen_t, char* __restrict, socklen_t, int);
struct netent* getnetbyaddr(uint32_t, int);
struct netent* getnetbyname(const char*);
struct netent* getnetent(void);
struct protoent* getprotobyname(const char*);
struct protoent* getprotobynumber(int);
struct protoent* getprotoent(void);
struct servent* getservbyname(const char*, const char*);
struct servent* getservbyport(int, const char*);
struct servent* getservent(void);
void sethostent(int);
void setnetent(int);
void setprotoent(int);
void setservent(int);

void herror(const char* s);

__END_DECLS

#endif // _NETDB_H
