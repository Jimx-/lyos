#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <ctype.h>

#include "types.h"
#include "proto.h"

void print_path(struct tcb* tcp, char* string, int len)
{
    if (!string) {
        print_addr((uint64_t)string);
        return;
    }

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
    printf("makedev(%lx, %lx)", MAJOR(dev), MINOR(dev));
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

int print_str(struct tcb* tcp, const void* addr, size_t len,
              unsigned int user_style)
{
    static char* str;
    static char* outstr;

    unsigned int size;
    unsigned int style = user_style;
    int rc = 0;
    int ellipsis;

    if (!addr) {
        printf("NULL");
        return -1;
    }

    if (!str) {
        const unsigned int outstr_size = 4 * max_strlen + 3;

        str = malloc(max_strlen + 1);
        outstr = malloc(outstr_size);
    }

    size = max_strlen + 1;

    if (size > len) size = len;

    fetch_mem(tcp, addr, str, size);
    if (style & QUOTE_0_TERMINATED) str[size] = '\0';

    if (rc < 0) {
        print_addr((uint64_t)addr);
        return rc;
    }

    if (size > max_strlen)
        size = max_strlen;
    else
        str[size] = '\xff';

    ellipsis = string_quote(str, outstr, size, style, NULL) && len &&
               ((style & (QUOTE_0_TERMINATED | QUOTE_EXPECT_TRAILING_0)) ||
                len > max_strlen);

    printf("%s", outstr);
    if (ellipsis) printf("...");

    return rc;
}

int string_quote(const char* instr, char* outstr, size_t size,
                 unsigned int style, const char* escape_chars)
{
    const unsigned char* ustr = (const unsigned char*)instr;
    char* s = outstr;
    unsigned int i;
    int usehex, c, eol;
    int printable;

    if (style & QUOTE_0_TERMINATED)
        eol = '\0';
    else
        eol = 0x100;

    usehex = !!(style & QUOTE_FORCE_HEX);

    if (style & QUOTE_EMIT_COMMENT) s = stpcpy(s, " /* ");
    if (!(style & QUOTE_OMIT_LEADING_TRAILING_QUOTES)) *s++ = '\"';

    if (usehex) {
        for (i = 0; i < size; ++i) {
            c = ustr[i];
            if (c == eol) goto asciz_ended;
            *s++ = '\\';
            *s++ = 'x';
            s += sprintf(s, "%02x", c);
        }

        goto string_ended;
    }

    for (i = 0; i < size; ++i) {
        c = ustr[i];

        if (c == eol) goto asciz_ended;
        if ((i == (size - 1)) && (style & QUOTE_OMIT_TRAILING_0) && (c == '\0'))
            goto asciz_ended;
        switch (c) {
        case '\"':
        case '\\':
            *s++ = '\\';
            *s++ = c;
            break;
        case '\f':
            *s++ = '\\';
            *s++ = 'f';
            break;
        case '\n':
            *s++ = '\\';
            *s++ = 'n';
            break;
        case '\r':
            *s++ = '\\';
            *s++ = 'r';
            break;
        case '\t':
            *s++ = '\\';
            *s++ = 't';
            break;
        case '\v':
            *s++ = '\\';
            *s++ = 'v';
            break;
        default:
            printable = isprint(c);

            if (printable && escape_chars) printable = !strchr(escape_chars, c);

            if (printable) {
                *s++ = c;
            } else {
                *s++ = '\\';
                if (i + 1 < size && ustr[i + 1] >= '0' && ustr[i + 1] <= '7') {
                    *s++ = '0' + (c >> 6);
                    *s++ = '0' + ((c >> 3) & 0x7);
                } else {
                    if ((c >> 3) != 0) {
                        if ((c >> 6) != 0) *s++ = '0' + (c >> 6);
                        *s++ = '0' + ((c >> 3) & 0x7);
                    }
                }
                *s++ = '0' + (c & 0x7);
            }
        }
    }

string_ended:
    if (!(style & QUOTE_OMIT_LEADING_TRAILING_QUOTES)) *s++ = '\"';
    if (style & QUOTE_EMIT_COMMENT) {
        *s++ = ' ';
        *s++ = '*';
        *s++ = '/';
    }
    *s = '\0';

    if (style & QUOTE_0_TERMINATED && ustr[i] == '\0') {
        return 0;
    }

    return 1;

asciz_ended:
    if (!(style & QUOTE_OMIT_LEADING_TRAILING_QUOTES)) *s++ = '\"';
    if (style & QUOTE_EMIT_COMMENT) {
        *s++ = ' ';
        *s++ = '*';
        *s++ = '/';
    }
    *s = '\0';

    return 0;
}
