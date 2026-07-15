#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include <errno.h>
#include <stdlib.h>
#include "string.h"
#include <lyos/acpi_utils.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>

#include "pci.h"
#include "pci_ecam.h"

int pci_acpi_init(void)
{
    int count;
    int i;
    int buses = 0;

    count = acpi_get_mcfg_count();
    if (count <= 0) return 0;

    for (i = 0; i < count; i++) {
        struct acpi_mcfg_entry entry;
        struct pci_config_window* cfg;
        struct pcibus* bus;
        unsigned int nr_buses;
        unsigned long base;
        size_t size;
        int retval;

        retval = acpi_get_mcfg_entry(i, &entry);
        if (retval) continue;

        if (entry.segment != 0) {
            printl("pci: skipping ACPI MCFG segment %u\n", entry.segment);
            continue;
        }

        if (entry.end_bus < entry.start_bus) continue;

        nr_buses = entry.end_bus - entry.start_bus + 1;
        base = (unsigned long)entry.base_addr +
               ((unsigned long)entry.start_bus << PCIE_ECAM_BUS_SHIFT);
        size = (size_t)nr_buses << PCIE_ECAM_BUS_SHIFT;

        cfg = pci_ecam_create(base, size);
        if (!cfg) {
            printl("pci: failed to map ACPI MCFG %u:%u-%u @ %08lx\n",
                   entry.segment, entry.start_bus, entry.end_bus, base);
            continue;
        }

        pci_ecam_set_bus_range(cfg, entry.start_bus);

        bus = pci_scan_bus(entry.start_bus, &pci_generic_ecam_ops, cfg);
        if (!bus) {
            pci_ecam_free(cfg);
            continue;
        }

        printl("pci: ACPI MCFG segment %u bus %u-%u @ %08lx\n", entry.segment,
               entry.start_bus, entry.end_bus, base);
        buses++;
    }

    return buses;
}
