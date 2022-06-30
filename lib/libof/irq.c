#include <lyos/types.h>
#include <errno.h>
#include <lyos/irqctl.h>

#include <libfdt/libfdt.h>
#include "libof.h"

int of_irq_find_parent(const void* blob, unsigned long offset,
                       u32* parent_handle, unsigned long* parent_offset)
{
    u32 parent;
    const void* prop;
    int ret;

    do {
        prop = fdt_getprop(blob, offset, "interrupt-parent", NULL);
        if (!prop) {
            int parent_offset = fdt_parent_offset(blob, offset);
            if (parent_offset < 0)
                ret = -ENOENT;
            else
                offset = parent_offset;
        } else {
            parent = of_read_number(prop, 1);
            ret = of_find_node_by_phandle(blob, parent, &offset);
        }
    } while (ret == 0 &&
             fdt_getprop(blob, offset, "#interrupt-cells", NULL) == NULL);

    *parent_handle = parent;
    if (parent_offset) *parent_offset = offset;

    return ret;
}

int of_irq_count(const void* blob, unsigned long offset)
{
    struct of_phandle_args irq;
    int nr = 0;

    while (of_irq_parse_one(blob, offset, nr, &irq) == 0)
        nr++;

    return nr;
}

int of_irq_parse_one(const void* blob, unsigned long offset, int index,
                     struct of_phandle_args* out_irq)
{
    u32 parent;
    unsigned long parent_offset;
    int intsize, len;
    const u32* prop;
    int i, retval;

    retval = of_parse_phandle_with_args(blob, offset, "interrupts-extended",
                                        "#interrupt-cells", index, out_irq);
    if (!retval) return 0;

    retval = of_irq_find_parent(blob, offset, &parent, &parent_offset);
    if (retval) return retval;

    prop = fdt_getprop(blob, parent_offset, "#interrupt-cells", NULL);
    if (!prop) return -EINVAL;
    intsize = of_read_number(prop, 1);

    prop = fdt_getprop(blob, offset, "interrupts", &len);
    if (!prop) return -EINVAL;

    if (len < intsize * (index + 1) * sizeof(u32)) return -EOVERFLOW;

    out_irq->phandle = parent;
    out_irq->offset = parent_offset;
    prop += intsize * index;
    out_irq->args_count = intsize;
    for (i = 0; i < intsize; i++) {
        out_irq->args[i] = of_read_number(prop++, 1);
    }

    return 0;
}

unsigned int irq_of_map(struct of_phandle_args* oirq)
{
    struct irqctl_fwspec fwspec;
    int i;

    fwspec.fwid = oirq->phandle;
    fwspec.param_count = oirq->args_count;
    for (i = 0; i < oirq->args_count; i++)
        fwspec.param[i] = oirq->args[i];

    return irqctl_map_fwspec(&fwspec);
}

unsigned int irq_of_parse_and_map(const void* blob, unsigned long offset,
                                  int index)
{
    struct of_phandle_args irq;

    if (of_irq_parse_one(blob, offset, index, &irq)) return 0;

    return irq_of_map(&irq);
}
