#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include <stdlib.h>
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include <lyos/portio.h>
#include <lyos/service.h>
#include <asm/io.h>
#include <lyos/vm.h>
#include <sys/mman.h>

#include <asm/pci.h>
#include "pci.h"
#include "pci_ecam.h"

struct pci_config_window* pci_ecam_create(unsigned long base, size_t size)
{
    struct pci_config_window* cfg;

    cfg = malloc(sizeof(*cfg));
    if (!cfg) return NULL;

    memset(cfg, 0, sizeof(*cfg));

    cfg->win = mm_map_phys(SELF, (void*)base, size);
    if (cfg->win == MAP_FAILED) {
        free(cfg);
        return NULL;
    }

    return cfg;
}

void pci_ecam_free(struct pci_config_window* cfg) { free(cfg); }

int pci_generic_config_read(struct pcibus* bus, unsigned int devfn, int where,
                            int size, u32* val)
{
    int busnr = bus->busnr;
    struct pci_config_window* cfg = bus->private;
    void* addr = cfg->win + PCIE_ECAM_OFFSET(busnr, devfn, where);

    if (size == 1)
        *val = (u32)readb(addr);
    else if (size == 2)
        *val = (u32)readw(addr);
    else
        *val = readl(addr);

    return 0;
}

int pci_generic_config_write(struct pcibus* bus, unsigned int devfn, int where,
                             int size, uint32_t val)
{
    int busnr = bus->busnr;
    struct pci_config_window* cfg = bus->private;
    void* addr = cfg->win + PCIE_ECAM_OFFSET(busnr, devfn, where);

    if (size == 1)
        writeb(addr, val);
    else if (size == 2)
        writew(addr, val);
    else
        writel(addr, val);

    return 0;
}

#define PCI_OP_READ(size, type, len)                                        \
    type pci_bus_read_config_##size(struct pcibus* bus, unsigned int devfn, \
                                    int pos)                                \
    {                                                                       \
        uint32_t data = 0;                                                  \
        pci_generic_config_read(bus, devfn, pos, len, &data);               \
        return data;                                                        \
    }

#define PCI_OP_WRITE(size, type, len)                                        \
    void pci_bus_write_config_##size(struct pcibus* bus, unsigned int devfn, \
                                     int pos, type value)                    \
    {                                                                        \
        pci_generic_config_write(bus, devfn, pos, len, value);               \
    }

PCI_OP_READ(byte, u8, 1)
PCI_OP_READ(word, u16, 2)
PCI_OP_READ(dword, u32, 4)
PCI_OP_WRITE(byte, u8, 1)
PCI_OP_WRITE(word, u16, 2)
PCI_OP_WRITE(dword, u32, 4)

const struct pci_ops pci_generic_ecam_ops = {
    .rreg_u8 = pci_bus_read_config_byte,
    .rreg_u16 = pci_bus_read_config_word,
    .rreg_u32 = pci_bus_read_config_dword,
    .wreg_u8 = pci_bus_write_config_byte,
    .wreg_u16 = pci_bus_write_config_word,
    .wreg_u32 = pci_bus_write_config_dword,
};
