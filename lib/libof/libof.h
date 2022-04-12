#ifndef _LIBOF_H_
#define _LIBOF_H_

#include <lyos/types.h>
#include <asm/byteorder.h>

typedef u32 phandle_t;

extern void* fdt_root;

#define MAX_PHANDLE_ARGS 16
struct of_phandle_args {
    unsigned long offset;
    int args_count;
    uint32_t args[MAX_PHANDLE_ARGS];
};

struct of_phandle_iterator {
    const char* cells_name;
    int cell_count;
    const void* blob;
    unsigned long parent_offset;

    const __be32* list_end;
    const __be32* phandle_end;

    const __be32* cur;
    u32 cur_count;
    phandle_t phandle;
    unsigned long offset;
};

static inline u64 of_read_number(const __be32* cell, int size)
{
    u64 r = 0;
    while (size--) {
        r = (r << 32) | be32_to_cpup(cell++);
    }
    return r;
}

int of_scan_fdt(int (*scan)(void*, unsigned long, const char*, int, void*),
                void* arg, void* blob);

int of_flat_dt_match(const void* blob, unsigned long node,
                     const char* const* compat);

int of_phandle_iterator_init(struct of_phandle_iterator* it, const void* blob,
                             unsigned long offset, const char* list_name,
                             const char* cells_name, int cell_count);
int of_phandle_iterator_next(struct of_phandle_iterator* it);

#define of_for_each_phandle(it, err, blob, off, ln, cn, cc)               \
    for (of_phandle_iterator_init((it), (blob), (off), (ln), (cn), (cc)), \
         err = of_phandle_iterator_next(it);                              \
         err == 0; err = of_phandle_iterator_next(it))

int of_irq_count(const void* blob, unsigned long offset);
int of_irq_parse_one(const void* blob, unsigned long offset, int index,
                     struct of_phandle_args* out_irq);

#endif // _LIBOF_H_
