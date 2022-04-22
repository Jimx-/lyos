#include <lyos/types.h>
#include <lyos/const.h>
#include <errno.h>
#include <string.h>

#include <libfdt/libfdt.h>
#include "libof.h"

struct of_bus {
    const char* name;
    const char* addresses;
    int (*match)(const void* blob, unsigned long offset);
    void (*count_cells)(const void* blob, unsigned long offset, int* addrc,
                        int* sizec);
    u64 (*map)(__be32* addr, const __be32* range, int na, int ns, int pna);
    int (*translate)(__be32* addr, u64 offset, int na);
};

static void of_bus_default_count_cells(const void* blob, unsigned long offset,
                                       int* addrc, int* sizec)
{
    of_n_addr_size_cells(blob, offset, addrc, sizec);
}

static u64 of_bus_default_map(__be32* addr, const __be32* range, int na, int ns,
                              int pna)
{
    u64 cp, s, da;

    cp = of_read_number(range, na);
    s = of_read_number(range + na + pna, ns);
    da = of_read_number(addr, na);

    if (da < cp || da >= (cp + s)) return OF_BAD_ADDR;
    return da - cp;
}

static int of_bus_default_translate(__be32* addr, u64 offset, int na)
{
    u64 a = of_read_number(addr, na);
    memset(addr, 0, na * 4);
    a += offset;
    if (na > 1) addr[na - 2] = cpu_to_be32(a >> 32);
    addr[na - 1] = cpu_to_be32(a & 0xffffffffu);

    return 0;
}

static struct of_bus of_busses[] = {{
    .name = "default",
    .addresses = "reg",
    .count_cells = of_bus_default_count_cells,
    .map = of_bus_default_map,
    .translate = of_bus_default_translate,
}};

static struct of_bus* of_match_bus(const void* blob, unsigned long offset)
{
    int i;

    for (i = 0; i < sizeof(of_busses) / sizeof(of_busses[0]); i++)
        if (!of_busses[i].match || of_busses[i].match(blob, offset))
            return &of_busses[i];
    return NULL;
}

const __be32* of_get_address(const void* blob, unsigned long offset, int index,
                             phys_bytes* sizep)
{
    struct of_bus* bus;
    int parent_offset;
    int na, ns;
    const __be32* prop;
    int psize, i, onesize;

    parent_offset = fdt_parent_offset(blob, offset);
    if (parent_offset < 0) return NULL;

    bus = of_match_bus(blob, parent_offset);

    bus->count_cells(blob, offset, &na, &ns);
    if (na <= 0 || na > OF_MAX_ADDR_CELLS) return NULL;

    prop = fdt_getprop(blob, offset, bus->addresses, &psize);
    if (!prop) return NULL;
    psize /= 4;

    onesize = na + ns;
    for (i = 0; psize >= onesize; psize -= onesize, prop += onesize, i++) {
        if ((index >= 0) && (i == index)) {
            if (sizep) *sizep = of_read_number(prop + na, ns);
            return prop;
        }
    }

    return NULL;
}

static int of_translate_one(const void* blob, unsigned long offset,
                            struct of_bus* bus, struct of_bus* pbus,
                            __be32* addr, int na, int ns, int pna,
                            const char* rprop)
{
    const __be32* ranges;
    u64 map_addr;
    int rlen, rone;

    ranges = fdt_getprop(blob, offset, rprop, &rlen);
    if (!ranges && strcmp(rprop, "dma-ranges")) {
        return 1;
    }
    if (rlen == 0) {
        map_addr = of_read_number(addr, na);
        memset(addr, 0, pna * 4);
        goto finish;
    }

    rlen /= 4;
    rone = na + pna + ns;
    for (; rlen >= rone; rlen -= rone, ranges += rone) {
        map_addr = bus->map(addr, ranges, na, ns, pna);
        if (map_addr != OF_BAD_ADDR) break;
    }

    if (map_addr == OF_BAD_ADDR) return 1;

    memcpy(addr, ranges + na, 4 * pna);

finish:
    return pbus->translate(addr, map_addr, pna);
}

u64 of_translate_address(const void* blob, unsigned long offset,
                         const __be32* in_addr, const char* rprop)
{
    struct of_bus *bus, *pbus;
    int parent_offset;
    __be32 addr[OF_MAX_ADDR_CELLS];
    int na, ns, pna, pns;
    u64 result = OF_BAD_ADDR;

    parent_offset = fdt_parent_offset(blob, offset);
    if (parent_offset < 0) return result;

    bus = of_match_bus(blob, parent_offset);

    bus->count_cells(blob, offset, &na, &ns);
    if (na <= 0 || na > OF_MAX_ADDR_CELLS) return result;

    memcpy(addr, in_addr, na * 4);

    for (;;) {
        offset = parent_offset;
        parent_offset = fdt_parent_offset(blob, offset);

        if (parent_offset < 0) {
            result = of_read_number(addr, na);
            break;
        }

        pbus = of_match_bus(blob, parent_offset);
        pbus->count_cells(blob, offset, &pna, &pns);
        if (pna <= 0 || pna > OF_MAX_ADDR_CELLS) break;

        if (of_translate_one(blob, offset, bus, pbus, addr, na, ns, pna,
                             rprop)) {
        }
        na = pna;
        ns = pns;
        bus = pbus;
    }

    return result;
}

int of_address_parse_one(const void* blob, unsigned long offset, int index,
                         phys_bytes* basep, phys_bytes* sizep)
{
    phys_bytes size;
    const __be32* addrp;
    u64 taddr;

    addrp = of_get_address(blob, offset, index, &size);
    if (!addrp) return -EINVAL;

    taddr = of_translate_address(blob, offset, addrp, "ranges");

    if (taddr == OF_BAD_ADDR) return -EINVAL;

    if (basep) *basep = taddr;
    if (sizep) *sizep = size;

    return 0;
}
