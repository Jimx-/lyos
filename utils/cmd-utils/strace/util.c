#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/ptrace.h>
#include <errno.h>

#include "types.h"
#include "proto.h"

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

int fetch_mem(struct tcb* tcp, const void* addr, void* buf, size_t size)
{
    static char* dest_buf;
    static size_t dest_buf_size;
    uintptr_t src_ptr = (uintptr_t)addr;
    long *src, *dest;
    off_t offset, len;

    len = size;
    offset = src_ptr % sizeof(long);
    src_ptr -= offset;
    size += offset;
    if (size % sizeof(long)) size += sizeof(long) - (size % sizeof(long));
    src = (long*)src_ptr;

    if (!dest_buf) {
        dest_buf_size = size;
        dest_buf = malloc(size);
    } else if (dest_buf_size < size) {
        free(dest_buf);
        dest_buf_size = size;
        dest_buf = malloc(size);
    }

    if (!dest_buf) {
        dest_buf_size = 0;
        return 0;
    }

    dest = (long*)dest_buf;

    while (src < (long*)(src_ptr + size)) {
        long data = ptrace(PTRACE_PEEKDATA, tcp->pid, src, 0);
        if (data == -1 && errno != 0) return 0;
        *dest = data;
        src++;
        dest++;
    }

    memcpy(buf, &dest_buf[offset], len);

    return 1;
}

int print_array(struct tcb* tcp, const void* start, size_t count,
                void* elem_buf, size_t elem_size, fetch_mem_fn fetch_fn,
                print_fn print_elem_fn, void* arg)
{
    size_t size;
    const void *cur, *end;
    int truncated = 0;

    if (!start) {
        printf("NULL");
        return 0;
    }

    if (!count) {
        printf("[]");
        return 0;
    }

    size = count * elem_size;
    end = start + size;

    if (end <= start || size / elem_size != count) {
        if (fetch_fn)
            print_addr((uint64_t)start);
        else
            printf("???");
        return 0;
    }

    for (cur = start; cur < end; cur += elem_size) {
        if (cur != start) printf(", ");

        if (fetch_fn) {
            if (!fetch_fn(tcp, cur, elem_buf, elem_size)) {
                if (cur == start)
                    print_addr((uint64_t)cur);
                else {
                    printf("...");
                    truncated = 1;
                }

                break;
            }
        } else
            elem_buf = (void*)cur;

        if (cur == start) printf("[");

        if (!print_elem_fn(tcp, elem_buf, elem_size, arg)) {
            cur = end;
            break;
        }
    }

    if ((cur != start) || !fetch_fn) printf("]");

    return cur >= end;
}
