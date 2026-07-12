/*
 * xHCI PCI glue - mirrors the ohci-pci.c pattern.
 * Initializes the xHCI hc_driver and delegates to the generic
 * PCI HCD probe function.
 */
#include <lyos/types.h>
#include <sys/types.h>
#include <lyos/sysutils.h>
#include <asm/io.h>

#include "hcd.h"
#include "xhci.h"

static struct hc_driver xhci_pci_hc_driver;

int xhci_pci_init(void)
{
    xhci_init_driver(&xhci_pci_hc_driver);
    return 0;
}

int xhci_pci_probe(int devind)
{
    return usb_hcd_pci_probe(devind, &xhci_pci_hc_driver);
}
