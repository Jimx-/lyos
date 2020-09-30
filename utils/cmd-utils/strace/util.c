#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/ptrace.h>

#include "types.h"

void print_path(struct tcb* tcp, char* string, int len)
{
    char* buf = (char*)malloc(len + 5);
    if (!buf) return;

    long* src = (long*)string;
    long* dest = (long*)buf;

    while (src < (long*)(string + len)) {
        long data = ptrace(PTRACE_PEEKDATA, tcp->pid, src, 0);
        if (data == -1) return;
        *dest = data;
        src++;
        dest++;
    }

    buf[len] = '\0';

    printf("\"%s\"", buf);

    free(buf);
}

void print_str(struct tcb* tcp, char* str, int len)
{
#define DEFAULT_BUF_LEN 32
    int truncated = 0;
    if (len > DEFAULT_BUF_LEN) {
        truncated = 1;
        len = DEFAULT_BUF_LEN;
    }

    char* buf = malloc(len + 1);
    if (!buf) return;

    long* src = (long*)str;
    long* dest = (long*)buf;

    while (src < (long*)(str + len)) {
        long data = ptrace(PTRACE_PEEKDATA, tcp->pid, src, 0);
        if (data == -1) goto out;
        *dest = data;
        src++;
        dest++;
    }

    buf[len] = '\0';
    printf("\"%s\"%s", buf, truncated ? "..." : "");

out:
    free(buf);
}

void print_err(int err) { printf("%d %s", err, strerror(err)); }

void print_addr(const uint64_t addr)
{
    if (!addr)
        printf("NULL");
    else
        printf("0x%" PRIx64, addr);
}

void print_dev_t(uint64_t dev)
{
    printf("makedev(%x, %x)", MAJOR(dev), MINOR(dev));
}
