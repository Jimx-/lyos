#include <netdb.h>
#include <stddef.h>

static int __h_errno;

int* __h_errno_location(void) { return &__h_errno; }

struct hostent* gethostbyname(const char* name) { return NULL; }

struct hostent* gethostbyaddr(const void* addr, socklen_t len, int type)
{
    return NULL;
}

struct netent* getnetent(void) { return NULL; }

struct netent* getnetbyname(const char* name) { return NULL; }

struct netent* getnetbyaddr(uint32_t net, int type) { return NULL; }

void setnetent(int stayopen) {}

void endnetent(void) {}

struct servent* getservent(void) { return NULL; }

struct servent* getservbyname(const char* name, const char* proto)
{
    return NULL;
}

struct servent* getservbyport(int port, const char* proto) { return NULL; }

void setservent(int stayopen) {}

void endservent(void) {}

struct protoent* getprotoent(void) { return NULL; }

struct protoent* getprotobyname(const char* name) { return NULL; }

struct protoent* getprotobynumber(int proto) { return NULL; }

void setprotoent(int stayopen) {}

void endprotoent(void) {}

void herror(const char* s) {}
