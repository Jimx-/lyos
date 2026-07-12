#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <lyos/sysutils.h>
#include <lyos/pci_utils.h>
#include <asm/pci.h>
#include <lyos/vm.h>
#include <asm/io.h>
#include <asm/protect.h>

#include "hcd.h"

#if CONFIG_USB_OHCI_HCD
#include "ohci.h"
#endif

#if CONFIG_USB_XHCI_HCD
#include "xhci.h"
#endif

struct pci_id {
    u16 vid;
    u16 did;
    u32 class_code;
    int (*probe)(int devind);
};

#define PCI_CLASS_USB_XHCI 0x0C0330

static const struct pci_id id_table[] = {
#if CONFIG_USB_OHCI_HCD_PCI
    {
        0x106b,
        0x003f,
        0,
        ohci_pci_probe,
    }
#endif
#if CONFIG_USB_XHCI_HCD_PCI
    ,
    {
        0xFFFF,
        0xFFFF,
        PCI_CLASS_USB_XHCI,
        xhci_pci_probe,
    }
#endif
};

int hcd_pci_probe(int devind, u16 vid, u16 did)
{
    u32 class_code;
    int i;

    class_code = pci_attr_r32(devind, PCI_REV) >> 8;

    for (i = 0; i < sizeof(id_table) / sizeof(id_table[0]); i++) {
        const struct pci_id* pci_id = &id_table[i];

        if (pci_id->class_code) {
            if ((class_code & 0xFFFFFF) == pci_id->class_code) {
                return pci_id->probe(devind);
            }
        } else if (pci_id->vid == vid && pci_id->did == did) {
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
#if CONFIG_USB_XHCI_HCD_PCI
    xhci_pci_init();
#endif
}

int usb_hcd_pci_probe(int devind, const struct hc_driver* driver)
{
    struct usb_hcd* hcd;
    int hcd_irq = 0;
    u16 cmd;
    int retval = 0;

    /* Enable bus mastering and memory space */
    cmd = pci_attr_r16(devind, PCI_CR);
    cmd |= PCI_CR_MAST_EN | PCI_CR_MEM_EN;
    pci_attr_w16(devind, PCI_CR, cmd);

    hcd = usb_create_hcd(driver);
    if (!hcd) {
        return ENOMEM;
    }

    /* Map MMIO BAR */
    if (driver->flags & HCD_MEMORY) {
        unsigned long bar_base;
        size_t bar_size;
        int ioflag;

        retval = pci_get_bar(devind, PCI_BAR, &bar_base, &bar_size, &ioflag);
        if (retval) goto put_hcd;

        hcd->regs = mm_map_phys(SELF, bar_base, bar_size, MMP_IO);
        if (!hcd->regs) {
            retval = ENOMEM;
            goto put_hcd;
        }

        /* Try MSI/MSI-X for USB3 controllers using the mapped BAR */
        if ((driver->flags & HCD_MASK) >= HCD_USB3) {
            int msi_pos = 0, msix_pos = 0;
            u8 cap_ptr = pci_attr_r8(devind, PCI_CAPPTR);

            while (cap_ptr >= 0x40) {
                u8 cap_id = pci_attr_r8(devind, cap_ptr);
                u8 next = pci_attr_r8(devind, cap_ptr + 1) & 0xfc;
                if (cap_id == PCI_CAP_ID_MSI) msi_pos = cap_ptr;
                if (cap_id == PCI_CAP_ID_MSIX) msix_pos = cap_ptr;
                if (cap_id == 0xff || next == 0) break;
                cap_ptr = next;
            }

            if (msix_pos > 0) {
                u16 msix_ctrl = pci_attr_r16(devind, msix_pos + 2);
                u32 table_off = pci_attr_r32(devind, msix_pos + 4) & ~0x7u;
                volatile u32* msix_tbl;

                /* MSI-X table is within the already-mapped BAR */
                msix_tbl = (volatile u32*)((char*)hcd->regs + table_off);

                hcd_irq = 16;
                msix_tbl[0] = MSI_ADDR_BASE_LO;
                msix_tbl[1] = 0;
                msix_tbl[2] = INT_VECTOR_IRQ0 + hcd_irq;
                msix_tbl[3] = 0;

                msix_ctrl |= 1 << 15;
                msix_ctrl &= ~(1 << 14);
                pci_attr_w16(devind, msix_pos + 2, msix_ctrl);

                cmd = pci_attr_r16(devind, PCI_CR);
                cmd |= PCI_CR_INT_DIS;
                pci_attr_w16(devind, PCI_CR, cmd);
            } else if (msi_pos > 0) {
                u16 msi_ctrl = pci_attr_r16(devind, msi_pos + PCI_MSI_FLAGS);
                int is_64bit = (msi_ctrl & PCI_MSI_FLAGS_64BIT) != 0;

                hcd_irq = 16;
                pci_attr_w32(devind, msi_pos + PCI_MSI_ADDRESS_LO,
                             MSI_ADDR_BASE_LO);
                if (is_64bit)
                    pci_attr_w32(devind, msi_pos + PCI_MSI_ADDRESS_HI, 0);

                u16 msi_data = INT_VECTOR_IRQ0 + hcd_irq;
                if (is_64bit)
                    pci_attr_w16(devind, msi_pos + PCI_MSI_DATA_64, msi_data);
                else
                    pci_attr_w16(devind, msi_pos + PCI_MSI_DATA_32, msi_data);

                msi_ctrl |= PCI_MSI_FLAGS_ENABLE;
                pci_attr_w16(devind, msi_pos + PCI_MSI_FLAGS, msi_ctrl);

                cmd = pci_attr_r16(devind, PCI_CR);
                cmd |= PCI_CR_INT_DIS;
                pci_attr_w16(devind, PCI_CR, cmd);
            }
        }
    }

    if (hcd_irq == 0) hcd_irq = pci_attr_r8(devind, PCI_ILR);

    retval = usb_hcd_add(hcd, hcd_irq);
    if (retval) {
        printl("usbd: usb_hcd_add failed (%d)\n", retval);
        goto put_hcd;
    }

    return retval;

put_hcd:
    usb_put_hcd(hcd);

    return retval;
}
