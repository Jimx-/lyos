/*
 * xHCI root hub operations: port status reporting, port feature
 * set/clear, and port reset handling.
 *
 * Unlike OHCI/EHCI where the root hub is emulated by reading/writing
 * roothub register sets, xHCI exposes port status through the PORTSC
 * registers in the operational register space. The root hub is still
 * emulated at the USB level (the generic HCD layer handles GetHubDescriptor,
 * etc.), but port status/control goes through PORTSC.
 */
#include <lyos/types.h>
#include <lyos/const.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <lyos/usb.h>

#include "hcd.h"
#include "xhci.h"

/* Read the raw port status and convert to USB hub status format */
static u32 xhci_get_port_status(struct xhci_hcd* xhci, int port)
{
    volatile u32* portsc = xhci_portsc_addr(xhci, port);
    u32 raw = xhci_readl(xhci, portsc);
    u32 status = 0;

    /* Current connect status */
    if (raw & PORTSC_CCS) status |= (1 << 0); /* USB_PORT_STAT_CONNECTION */

    /* Port enabled */
    if (raw & PORTSC_PED) status |= (1 << 1); /* USB_PORT_STAT_ENABLE */

    /* Suspend (U3 state) */
    if (PORTSC_PLS(raw) == PLS_U3)
        status |= (1 << 2); /* USB_PORT_STAT_SUSPEND */

    /* Over-current */
    if (raw & PORTSC_OCA) status |= (1 << 3); /* USB_PORT_STAT_OVERCURRENT */

    /* Port reset */
    if (raw & PORTSC_PR) status |= (1 << 4); /* USB_PORT_STAT_RESET */

    /* Port power */
    if (raw & PORTSC_PP) status |= (1 << 8); /* USB_PORT_STAT_POWER */

    /* Low speed (USB 1.1) */
    if (PORTSC_SPEED(raw) == 2) status |= 0x0200; /* USB_PORT_STAT_LOW_SPEED */

    /* High speed (USB 2.0) */
    if (PORTSC_SPEED(raw) == 3) status |= 0x0400; /* USB_PORT_STAT_HIGH_SPEED */

    /* Change bits */
    if (raw & PORTSC_CSC) status |= (1 << 16); /* USB_PORT_STAT_C_CONNECTION */
    if (raw & PORTSC_PEC) status |= (1 << 17); /* USB_PORT_STAT_C_ENABLE */
    if (raw & PORTSC_OCC) status |= (1 << 19); /* USB_PORT_STAT_C_OVERCURRENT */
    if (raw & PORTSC_PRC) status |= (1 << 20); /* USB_PORT_STAT_C_RESET */
    if (raw & PORTSC_PLC) status |= (1 << 22); /* USB_PORT_STAT_C_SUSPEND */
    if (raw & PORTSC_WRC) status |= (1 << 23); /* USB_PORT_STAT_C_BH_RESET */
    if (raw & PORTSC_CEC)
        status |= (1 << 24); /* USB_PORT_STAT_C_CONFIG_ERROR */

    return status;
}

/* =========================================================================
 * hc_driver: hub_status_data
 *
 * Called by the generic HCD layer to check if any root hub port has
 * a status change. Returns a bitmask in buf (similar to OHCI).
 * ========================================================================= */
int xhci_hub_status_data(struct usb_hcd* hcd, char* buf)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    int i;
    int changed = 0;
    int length = 1;

    buf[0] = 0;
    if (xhci->num_ports > 7) {
        length = 2;
        buf[1] = 0;
    }

    for (i = 0; i < xhci->num_ports; i++) {
        volatile u32* portsc = xhci_portsc_addr(xhci, i);
        u32 val = xhci_readl(xhci, portsc);

        if (val & PORTSC_CHANGE_MASK) {
            changed = 1;
            if (i < 7)
                buf[0] |= 1 << (i + 1);
            else
                buf[1] |= 1 << (i - 7);
        }
    }

    return changed ? length : 0;
}

/* =========================================================================
 * hc_driver: hub_control
 *
 * Handle USB hub class requests directed at the root hub.
 * ========================================================================= */
int xhci_hub_control(struct usb_hcd* hcd, u16 typeReq, u16 wValue, u16 wIndex,
                     char* buf, u16 wLength)
{
    struct xhci_hcd* xhci = hcd_to_xhci(hcd);
    int ports = xhci->num_ports;
    u32 val;
    int retval = 0;

    switch (typeReq) {
    case GetHubStatus:
        /* Root hub doesn't have local power or over-current issues */
        memset(buf, 0, 4);
        retval = 4;
        break;

    case GetPortStatus:
        if (!wIndex || wIndex > ports) {
            retval = -EPIPE;
            break;
        }
        wIndex--; /* convert to 0-based */
        val = xhci_get_port_status(xhci, wIndex);
        *(u32*)buf = val;
        retval = 4;
        break;

    case ClearPortFeature:
        if (!wIndex || wIndex > ports) {
            retval = -EPIPE;
            break;
        }
        wIndex--;
        {
            volatile u32* portsc = xhci_portsc_addr(xhci, wIndex);
            val = xhci_readl(xhci, portsc);

            switch (wValue) {
            case USB_PORT_FEAT_ENABLE:
                xhci_writel(xhci, portsc, val & ~PORTSC_PED);
                break;
            case USB_PORT_FEAT_POWER:
                if (xhci->hcc_params & HCC_PPC(xhci->hcc_params))
                    xhci_writel(xhci, portsc, val & ~PORTSC_PP);
                break;
            case USB_PORT_FEAT_C_CONNECTION:
                xhci_writel(xhci, portsc, val | PORTSC_CSC);
                break;
            case USB_PORT_FEAT_C_ENABLE:
                xhci_writel(xhci, portsc, val | PORTSC_PEC);
                break;
            case USB_PORT_FEAT_C_SUSPEND:
                xhci_writel(xhci, portsc, val | PORTSC_PLC);
                break;
            case USB_PORT_FEAT_C_OVER_CURRENT:
                xhci_writel(xhci, portsc, val | PORTSC_OCC);
                break;
            case USB_PORT_FEAT_C_RESET:
                xhci_writel(xhci, portsc, val | PORTSC_PRC);
                break;
            case USB_PORT_FEAT_C_PORT_L1:
                /* L1 suspend change - mapped to PLC in xHCI */
                xhci_writel(xhci, portsc, val | PORTSC_PLC);
                break;
            default:
                retval = -EPIPE;
                break;
            }
        }
        break;

    case SetPortFeature:
        if (!wIndex || wIndex > ports) {
            retval = -EPIPE;
            break;
        }
        wIndex--;
        {
            volatile u32* portsc = xhci_portsc_addr(xhci, wIndex);
            val = xhci_readl(xhci, portsc);

            switch (wValue) {
            case USB_PORT_FEAT_POWER:
                if (xhci->hcc_params & HCC_PPC(xhci->hcc_params))
                    xhci_writel(xhci, portsc, val | PORTSC_PP);
                break;
            case USB_PORT_FEAT_RESET:
                /* Issue port reset. For USB 3.0 ports, use warm reset
                 * (WPR) if the port is in a compliance or error state.
                 * For USB 2.0 ports, a regular reset (PR) suffices. */
                if ((val & PORTSC_PR) == 0) {
                    /* Clear reset change bit before asserting reset */
                    if (val & PORTSC_PRC)
                        xhci_writel(xhci, portsc, val | PORTSC_PRC);
                    xhci_writel(xhci, portsc, val | PORTSC_PR);
                }
                /* TODO: wait for reset to complete and poll for
                 * PRC (port reset change) bit. The actual completion
                 * is typically handled asynchronously via the port
                 * change interrupt and the hub driver's port reset
                 * polling. */
                break;
            case USB_PORT_FEAT_SUSPEND:
                /* Put port into U3 (suspend) */
                xhci_writel(xhci, portsc,
                            val | PORTSC_LWS | PORTSC_PLS_SET(PLS_U3));
                break;
            case USB_PORT_FEAT_TEST:
                /* TODO: set test mode via PORTSC PLS = TEST_MODE */
                break;
            default:
                retval = -EPIPE;
                break;
            }
        }
        break;

    default:
        retval = -EPIPE;
        break;
    }

    return retval;
}

/* =========================================================================
 * Port change interrupt handler
 *
 * Called from xhci_irq() when STS_PCD is set. Polls all ports for
 * change bits and triggers root hub status polling.
 * ========================================================================= */
void xhci_handle_port_change(struct xhci_hcd* xhci)
{
    struct usb_hcd* hcd = xhci_to_hcd(xhci);
    unsigned int i;

    for (i = 0; i < xhci->num_ports; i++) {
        volatile u32* portsc = xhci_portsc_addr(xhci, i);
        u32 val = xhci_readl(xhci, portsc);

        if (val & PORTSC_CHANGE_MASK) {
            /* The generic HCD layer will call hub_status_data and
             * hub_control to process the changes. Just trigger the
             * root hub status URB completion. */
            usb_hcd_poll_rh_status(hcd);
            break;
        }
    }
}
