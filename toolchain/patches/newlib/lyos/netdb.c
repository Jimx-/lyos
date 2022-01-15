#include <netdb.h>
#include <stddef.h>
#include <sys/socket.h>
#include <stdio.h>
#include <resolv.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

static int __h_errno;

int* __h_errno_location(void) { return &__h_errno; }

#define RECORD_A     1
#define RECORD_CNAME 5
#define RECORD_PTR   12

struct dns_header {
    unsigned short id;
    unsigned short flags;
    unsigned short q_count;
    unsigned short ans_count;
    unsigned short auth_count;
    unsigned short add_count;
};

static int get_dns_servers(const char* dns_servers[2], char* buf, size_t buflen)
{
    FILE* fp;
    const char* p;
    char* pnext;
    int nr_servers = 0;
    size_t len;

    if ((fp = fopen(_PATH_RESCONF, "r")) == NULL) return -errno;

    dns_servers[0] = dns_servers[1] = NULL;

    while (fgets(buf, buflen, fp)) {
        if (buf[0] == '#') {
            continue;
        }

        if (strncmp(buf, "nameserver", 10) == 0) {
            p = strtok(buf, " ");
            p = strtok(NULL, " ");

            len = strlen(p);
            if (len >= buflen) {
                nr_servers = -ERANGE;
                break;
            }

            memmove(buf, p, len);
            dns_servers[nr_servers++] = buf;
            pnext = buf + len - 1;
            while (pnext > buf && (*pnext == '\r' || *pnext == '\n'))
                pnext--;
            pnext++;
            *pnext++ = '\0';
            buflen -= (pnext - buf);
            buf = pnext;

            if (nr_servers >= 2) break;
        }
    }

    fclose(fp);
    return nr_servers;
}

const char* read_dns_name(char* buf, char** endp)
{
    char* p;

    p = buf;
    while (*p) {
        char code = *p;

        if (!code) break;

        if ((code & 0xc0) == 0xc0) {
            p++;
            break;
        }

        *p++ = '.';
        p += code;
    }

    if (endp) *endp = p;

    return (p == buf) ? buf : &buf[1];
}

int gethostbyname_r(const char* name, struct hostent* ret, char* buf,
                    size_t buflen, struct hostent** result, int* h_errnop)
{
    const char *dns_servers[2], *end, *name_lim;
    char *p, *p_alloc;
    size_t left, recvlen;
    struct sockaddr_in sin;
    struct dns_header *dns, *resp;
    unsigned short dns_id;
    socklen_t addr_len;
    unsigned short no_ans;
    unsigned short n_addrs, n_aliases;
    int sock_fd;
    int i, retval;

#define SET_H_ERRNO(err)                 \
    do {                                 \
        if (h_errnop) *h_errnop = (err); \
    } while (0)

    if ((retval = get_dns_servers(dns_servers, buf, buflen)) <= 0) {
        SET_H_ERRNO(NO_RECOVERY);
        return (retval < 0 ? -retval : EINVAL);
    }

    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) goto out_err;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(53);
    sin.sin_addr.s_addr = inet_addr(dns_servers[0]);

    if (connect(sock_fd, (struct sockaddr*)&sin, sizeof(sin)) < 0)
        goto close_sock;

    p = buf;
    left = buflen;
    errno = ERANGE;

    if (left < sizeof(unsigned short)) goto close_sock;
    p += sizeof(unsigned short);
    left -= sizeof(unsigned short);

    if (left < sizeof(*dns)) {
        goto close_sock;
    }

    dns = (struct dns_header*)p;
    p += sizeof(*dns);
    left -= sizeof(*dns);

    memset(dns, 0, sizeof(*dns));
    dns_id = htons(123);
    dns->id = dns_id;
    dns->flags = htons(0x0100);
    dns->q_count = htons(1);
    dns->ans_count = 0;
    dns->auth_count = 0;
    dns->add_count = 0;

    end = name;
    name_lim = name + strlen(name);
    while (*end) {
        size_t len;

        end = strchr(name, '.');
        if (end == NULL) end = name_lim;
        len = end - name;
        if (left < len + 1) goto close_sock;
        *p++ = (char)len;
        memcpy(p, name, len);
        name += len + 1;
        p += len;
        left -= len + 1;
    }

    if (left < 2 * sizeof(unsigned short)) goto close_sock;
    *p++ = '\0';

    *(unsigned short*)p = htons(1);
    p += sizeof(unsigned short);
    *(unsigned short*)p = htons(1);
    p += sizeof(unsigned short);
    left -= 2 * sizeof(unsigned short) + 1;

    *(unsigned short*)buf = htons(p - buf - 2);

    if (sendto(sock_fd, buf, p - buf, 0, (struct sockaddr*)&sin, sizeof(sin)) <
        0)
        goto close_sock;

    addr_len = sizeof(sin);
    if ((recvlen = recvfrom(sock_fd, buf, buflen, 0, (struct sockaddr*)&sin,
                            &addr_len)) < 0)
        goto close_sock;

    if (recvlen < sizeof(unsigned short) + sizeof(*resp)) goto close_sock;

    p = &buf[sizeof(unsigned short)];

    p_alloc = buf + recvlen;
    left = buflen - recvlen;

    resp = (struct dns_header*)p;
    p += sizeof(*resp);
    left -= sizeof(*resp);

    if (resp->id != dns_id) {
        errno = EINVAL;
        goto close_sock;
    }

    ret->h_name = NULL;
    for (i = 0; i < ntohs(resp->q_count); i++) {
        size_t name_len;
        char* pend;

        name = read_dns_name(p, &pend);
        name_len = pend - name;

        if (!i) {
            if (left <= name_len) goto close_sock;
            memcpy(p_alloc, name, name_len);
            p_alloc[name_len] = '\0';
            ret->h_name = p_alloc;
            p_alloc += name_len + 1;
            left -= name_len;
        }

        p = pend;
        p += 1 + 2 * sizeof(unsigned short);
    }

    no_ans = ntohs(resp->ans_count);
    if (left < (sizeof(char*) * 2 * (no_ans + 1))) goto close_sock;

    ret->h_aliases = (char**)p_alloc;
    p_alloc += sizeof(char*) * (no_ans + 1);
    ret->h_addr_list = (char**)p_alloc;
    p_alloc += sizeof(char*) * (no_ans + 1);
    left -= sizeof(char*) * 2 * (no_ans + 1);

    n_aliases = 0;
    n_addrs = 0;

    for (i = 0; i < no_ans; i++) {
        char* pend;
        uint16_t rr_type;
        uint16_t rr_length;

        name = read_dns_name(p, &pend);
        p = pend + 1;

        rr_type = ntohs(*(unsigned short*)&p[0]);
        rr_length = ntohs(*(unsigned short*)&p[8]);
        p += 10;

        switch (rr_type) {
        case RECORD_A:
            if (left < rr_length) goto close_sock;
            ret->h_addr_list[n_addrs++] = p_alloc;
            memcpy(p_alloc, p, rr_length);
            p += rr_length;
            p_alloc += rr_length;
            left -= rr_length;
            break;
        }
    }

    ret->h_aliases[n_aliases] = NULL;
    ret->h_addr_list[n_addrs] = NULL;

    ret->h_length = 4;
    ret->h_addrtype = AF_INET;

    errno = 0;
    SET_H_ERRNO(0);
    if (result) *result = ret;

close_sock:
    close(sock_fd);

out_err:
    if (errno) {
        SET_H_ERRNO(NO_RECOVERY);
        if (result) *result = NULL;
    }

    return errno;
}

struct hostent* gethostbyname(const char* name)
{
    static struct hostent hostent, *result;
    static char buf[4096];
    int err;

    gethostbyname_r(name, &hostent, buf, sizeof(buf), &result, &err);

    if (!result) {
        h_errno = err;
        return NULL;
    }

    return result;
}

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

const char* gai_strerror(int errcode) { return "(null)"; }
