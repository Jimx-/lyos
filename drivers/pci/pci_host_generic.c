#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <lyos/portio.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>

#include <asm/pci.h>
#include "pci.h"
#include "pci_ecam.h"

#include <libfdt/libfdt.h>
#include <libof/libof.h>

extern void* boot_params;
extern int dt_root_addr_cells;
extern int dt_root_size_cells;

static void fdt_parse_pci_interrupt(void* blob, unsigned long offset,
                                    struct pcibus* bus)
{
    int addr_cells, intr_cells;
    const u32* prop;
    const u32 *imap, *imap_lim;
    int nr_imaps = 0;
    int len;

    if ((prop = fdt_getprop(blob, offset, "#address-cells", NULL)) != NULL) {
        addr_cells = be32_to_cpup(prop);
    }
    if ((prop = fdt_getprop(blob, offset, "#interrupt-cells", NULL)) != NULL) {
        intr_cells = be32_to_cpup(prop);
    }

    imap = fdt_getprop(blob, offset, "interrupt-map", &len);
    if (!imap) return;
    imap_lim = (u32*)((char*)imap + len);

    if ((prop = fdt_getprop(blob, offset, "interrupt-map-mask", NULL)) !=
        NULL) {
        int i;
        for (i = 0; i < 4; i++)
            bus->imask[i] = be32_to_cpup(&prop[i]);
    }

    while (imap < imap_lim) {
        unsigned long child_intr, child_intr_hi;
        struct of_phandle_args irq;
        unsigned int pin;
        int i;

        child_intr_hi = of_read_number(imap, 1);
        child_intr = of_read_number(imap + 1, 2);
        imap += addr_cells;
        pin = of_read_number(imap, intr_cells);
        imap += intr_cells;

        irq.phandle = of_read_number(imap++, 1);
        irq.args_count = intr_cells;
        for (i = 0; i < intr_cells; i++)
            irq.args[i] = of_read_number(imap++, 1);

        bus->imap[nr_imaps].child_intr[0] = child_intr_hi & 0xfffffff;
        bus->imap[nr_imaps].child_intr[1] = (child_intr >> 32) & 0xffffffff;
        bus->imap[nr_imaps].child_intr[2] = child_intr & 0xffffffff;
        bus->imap[nr_imaps].child_intr[3] = pin;

        bus->imap[nr_imaps].irq_nr = irq_of_map(&irq);
        nr_imaps++;
    }
}

static int fdt_scan_pci_host(void* blob, unsigned long offset, const char* name,
                             int depth, void* arg)
{
    const char* type = fdt_getprop(blob, offset, "device_type", NULL);
    struct pcibus* bus;
    struct pci_config_window* cfg;
    const u32 *reg, *ranges, *ranges_lim;
    int len;
    u64 base, size;
    u16 did, vid;

    if (!type || strcmp(type, "pci") != 0) return 0;

    reg = fdt_getprop(blob, offset, "reg", &len);
    if (!reg) return 0;

    base = of_read_number(reg, dt_root_addr_cells);
    reg += dt_root_addr_cells;
    size = of_read_number(reg, dt_root_size_cells);
    reg += dt_root_size_cells;

    printl("pci: ECAM: [mem 0x%016lx-0x%016lx]\r\n", base, base + size - 1);

    cfg = pci_ecam_create(base, size);
    if (!cfg) return 1;

    bus = pci_create_bus(0, &pci_generic_ecam_ops, cfg);
    if (!bus) {
        pci_ecam_free(cfg);
        return 1;
    }

    vid = bus->ops->rreg_u16(bus, 0, PCI_VID);
    did = bus->ops->rreg_u16(bus, 0, PCI_DID);

    if (vid == 0xffff && did == 0xffff) {
        pci_ecam_free(cfg);
        return 0;
    }

    ranges = fdt_getprop(blob, offset, "ranges", &len);
    if (!ranges) return 0;
    ranges_lim = (u32*)((char*)ranges + len);

    bus->nr_resources = 0;
    while (ranges < ranges_lim) {
        unsigned long child_addr_hi, child_addr;
        unsigned long parent_addr;
        size_t range_size;
        const char* range_type;
        int flags;

        child_addr_hi = of_read_number(ranges, 1);
        child_addr = of_read_number(ranges + 1, 2);
        ranges += 3;
        parent_addr = of_read_number(ranges, dt_root_addr_cells);
        ranges += dt_root_addr_cells;
        range_size = of_read_number(ranges, dt_root_size_cells);
        ranges += dt_root_size_cells;

        switch ((child_addr_hi >> 24) & 3) {
        case 1:
            range_type = "IO";
            flags = PBF_IO;
            break;
        case 2:
            range_type = "MEM";
            flags = 0;
            break;
        default:
            continue;
        }

        bus->resources[bus->nr_resources].flags = flags;
        bus->resources[bus->nr_resources].cpu_addr = parent_addr;
        bus->resources[bus->nr_resources].pci_addr = child_addr;
        bus->resources[bus->nr_resources].size = range_size;
        bus->resources[bus->nr_resources].alloc_offset = 0;
        bus->nr_resources++;

        printl("pci:  %6s %012lx..%012lx -> %012lx\r\n", range_type,
               parent_addr, parent_addr + range_size - 1, child_addr);
    }

    fdt_parse_pci_interrupt(blob, offset, bus);

    pci_probe_bus(bus);

    return 0;
}

void pci_host_generic_init(void)
{
    of_scan_fdt(fdt_scan_pci_host, NULL, boot_params);
}
