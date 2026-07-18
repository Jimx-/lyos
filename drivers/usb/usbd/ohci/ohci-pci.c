#include <lyos/types.h>
#include <sys/types.h>
#include <lyos/sysutils.h>
#include <asm/io.h>

#include "hcd.h"
#include "ohci.h"

static struct hc_driver ohci_pci_hc_driver;

int ohci_pci_init(void)
{
    ohci_init_driver(&ohci_pci_hc_driver);
    return 0;
}

int ohci_pci_probe(int devind)
{
    return usb_hcd_pci_probe(devind, &ohci_pci_hc_driver);
}
