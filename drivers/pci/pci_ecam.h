#ifndef _PCI_ECAM_H_
#define _PCI_ECAM_H_

#define PCIE_ECAM_BUS_SHIFT   20 /* Bus number */
#define PCIE_ECAM_DEVFN_SHIFT 12 /* Device and Function number */

#define PCIE_ECAM_BUS_MASK   0xff
#define PCIE_ECAM_DEVFN_MASK 0xff
#define PCIE_ECAM_REG_MASK   0xfff /* Limit offset to a maximum of 4K */

#define PCIE_ECAM_BUS(x)   (((x)&PCIE_ECAM_BUS_MASK) << PCIE_ECAM_BUS_SHIFT)
#define PCIE_ECAM_DEVFN(x) (((x)&PCIE_ECAM_DEVFN_MASK) << PCIE_ECAM_DEVFN_SHIFT)
#define PCIE_ECAM_REG(x)   ((x)&PCIE_ECAM_REG_MASK)

#define PCIE_ECAM_OFFSET(bus, devfn, where) \
    (PCIE_ECAM_BUS(bus) | PCIE_ECAM_DEVFN(devfn) | PCIE_ECAM_REG(where))

struct pci_config_window {
    void* win;
};

extern const struct pci_ops pci_generic_ecam_ops;

struct pci_config_window* pci_ecam_create(unsigned long base, size_t size);
void pci_ecam_free(struct pci_config_window* cfg);

#endif
