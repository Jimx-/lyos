#ifndef _LIBOF_H_
#define _LIBOF_H_

#include <lyos/types.h>
#include <asm/byteorder.h>

#define OF_BAD_ADDR       ((u64)-1)
#define OF_MAX_ADDR_CELLS 4

typedef u32 phandle_t;

#define MAX_PHANDLE_ARGS 16
struct of_phandle_args {
    u32 phandle;
    unsigned long offset;
    int args_count;
    u32 args[MAX_PHANDLE_ARGS];
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

void of_n_addr_size_cells(const void* blob, unsigned long offset, int* addrc,
                          int* sizec);

int of_scan_fdt(int (*scan)(void*, unsigned long, const char*, int, void*),
                void* arg, void* blob);

int of_flat_dt_match(const void* blob, unsigned long node,
                     const char* const* compat);

int of_find_node_by_phandle(const void* blob, phandle_t handle,
                            unsigned long* offp);

int of_phandle_iterator_init(struct of_phandle_iterator* it, const void* blob,
                             unsigned long offset, const char* list_name,
                             const char* cells_name, int cell_count);
int of_phandle_iterator_next(struct of_phandle_iterator* it);
int of_phandle_iterator_args(struct of_phandle_iterator* it, uint32_t* args,
                             int size);

#define of_for_each_phandle(it, err, blob, off, ln, cn, cc)               \
    for (of_phandle_iterator_init((it), (blob), (off), (ln), (cn), (cc)), \
         err = of_phandle_iterator_next(it);                              \
         err == 0; err = of_phandle_iterator_next(it))

/* Addresses */
int of_address_parse_one(const void* blob, unsigned long offset, int index,
                         phys_bytes* basep, phys_bytes* sizep);

/* IRQ */
int of_irq_count(const void* blob, unsigned long offset);
int of_irq_parse_one(const void* blob, unsigned long offset, int index,
                     struct of_phandle_args* out_irq);

#ifndef __kernel__
unsigned int irq_of_map(struct of_phandle_args* oirq);
unsigned int irq_of_parse_and_map(const void* blob, unsigned long offset,
                                  int index);
#endif

#endif // _LIBOF_H_
