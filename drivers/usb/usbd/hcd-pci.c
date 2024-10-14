#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <lyos/sysutils.h>
#include <lyos/pci_utils.h>
#include <asm/pci.h>
#include <lyos/vm.h>
#include <asm/io.h>

#include "hcd.h"

#if CONFIG_USB_OHCI_HCD
#include "ohci.h"
#endif

struct pci_id {
    u16 vid;
    u16 did;
    int (*probe)(int devind);
};

static const struct pci_id id_table[] = {
#if CONFIG_USB_OHCI_HCD_PCI
    {
        0x106b,
        0x003f,
        ohci_pci_probe,
    }
#endif
};

int hcd_pci_probe(int devind, u16 vid, u16 did)
{
    int i;

    for (i = 0; i < sizeof(id_table) / sizeof(id_table[0]); i++) {
        const struct pci_id* pci_id = &id_table[i];

        if (pci_id->vid == vid && pci_id->did == did) {
            return pci_id->probe(devind);
        }
    }

    return ENOSYS;
}

void hcd_pci_scan(void)
{
    int devind;
    u16 vid, did;
    int r;

    r = pci_first_dev(&devind, &vid, &did, NULL);

    while (r == 0) {
        hcd_pci_probe(devind, vid, did);

        r = pci_next_dev(&devind, &vid, &did, NULL);
    }
}

void hcd_pci_init(void)
{
#if CONFIG_USB_OHCI_HCD_PCI
    ohci_pci_init();
#endif
}

int usb_hcd_pci_probe(int devind, const struct hc_driver* driver)
{
    struct usb_hcd* hcd;
    int hcd_irq = 0;
    int retval = 0;

    printl("USB hcd pci probe %s\n", driver->description);

    if ((driver->flags & HCD_MASK) < HCD_USB3) {
        hcd_irq = pci_attr_r8(devind, PCI_ILR);
    }

    hcd = usb_create_hcd(driver);
    if (!hcd) {
        return ENOMEM;
    }

    if (driver->flags & HCD_MEMORY) {
        unsigned long bar_base;
        size_t bar_size;
        int ioflag;

        retval = pci_get_bar(devind, PCI_BAR, &bar_base, &bar_size, &ioflag);
        if (ioflag) {
            retval = EINVAL;
            goto put_hcd;
        }

        hcd->regs = mm_map_phys(SELF, bar_base, bar_size, MMP_IO);
        if (!hcd->regs) {
            retval = ENOMEM;
            goto put_hcd;
        }
    }

    retval = usb_hcd_add(hcd, hcd_irq);
    if (retval) goto put_hcd;

    return retval;

put_hcd:
    usb_put_hcd(hcd);

    return retval;
}
