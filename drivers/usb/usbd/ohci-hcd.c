#define __LINUX_ERRNO_EXTENSIONS__
#include <lyos/sysutils.h>
#include <asm/io.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <lyos/irqctl.h>
#include <lyos/usb.h>
#include <asm/barrier.h>

#include "hcd.h"
#include "ohci.h"

#define OHCI_CONTROL_INIT OHCI_CTRL_CBSR
#define OHCI_INTR_INIT                                              \
    (OHCI_INTR_MIE | OHCI_INTR_RHSC | OHCI_INTR_UE | OHCI_INTR_RD | \
     OHCI_INTR_WDH)

static const char hcd_name[] = "ohci_hcd";

static const int cc_to_error[16] = {
    /* No  Error  */ 0,
    /* CRC Error  */ EILSEQ,
    /* Bit Stuff  */ EPROTO,
    /* Data Togg  */ EILSEQ,
    /* Stall      */ EPIPE,
    /* DevNotResp */ ETIME,
    /* PIDCheck   */ EPROTO,
    /* UnExpPID   */ EPROTO,
    /* DataOver   */ EOVERFLOW,
    /* DataUnder  */ EIO,
    /* (for hw)   */ EIO,
    /* (for hw)   */ EIO,
    /* BufferOver */ ECOMM,
    /* BuffUnder  */ ENOSR,
    /* (for HCD)  */ EALREADY,
    /* (for HCD)  */ EALREADY};

static void urb_free_priv(struct ohci_hcd* ohci,
                          struct ohci_urb_priv* urb_priv);
static int ed_schedule(struct ohci_hcd* ohci, struct ed* ed);
static void ed_deschedule(struct ohci_hcd* ohci, struct ed* ed);

static int ohci_init(struct ohci_hcd* ohci)
{
    struct usb_hcd* hcd = ohci_to_hcd(ohci);

    ohci->regs = hcd->regs;

    ohci_writel(ohci, &ohci->regs->intrdisable, OHCI_INTR_MIE);

    if (ohci_readl(ohci, &ohci->regs->control) & OHCI_CTRL_RWC)
        ohci->hc_control |= OHCI_CTRL_RWC;

    if (ohci->num_ports == 0)
        ohci->num_ports = ohci_readl(ohci, &ohci->regs->roothub.a) & RH_A_NDP;

    if (ohci->hcca) return 0;

    ohci->hcca =
        mmap(NULL, sizeof(*ohci->hcca), PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (ohci->hcca == MAP_FAILED) return ENOMEM;

    if (umap(SELF, UMT_VADDR, (vir_bytes)ohci->hcca, sizeof(*ohci->hcca),
             &ohci->hcca_phys) != 0) {
        panic("%s: umap failed", hcd_name);
    }

    return 0;
}

static int ohci_run(struct ohci_hcd* ohci)
{
    u32 val, mask;
    int first = ohci->fminterval == 0;

    if (first) {
        val = ohci_readl(ohci, &ohci->regs->fminterval);
        ohci->fminterval = val & 0x3fff;
        ohci->fminterval |= FSMP(ohci->fminterval) << 16;
    }

    switch (ohci->hc_control & OHCI_CTRL_HCFS) {
    case OHCI_USB_OPER:
        val = 0;
        break;
    case OHCI_USB_SUSPEND:
    case OHCI_USB_RESUME:
        ohci->hc_control &= OHCI_CTRL_RWC;
        ohci->hc_control |= OHCI_USB_RESUME;
        val = 10000;
        break;
    default:
        ohci->hc_control &= OHCI_CTRL_RWC;
        ohci->hc_control |= OHCI_USB_RESET;
        val = 50000;
        break;
    }
    ohci_writel(ohci, &ohci->regs->control, ohci->hc_control);
    (void)ohci_readl(ohci, &ohci->regs->control);
    usleep(val);

    memset(ohci->hcca, 0, sizeof(*ohci->hcca));

    ohci_writel(ohci, &ohci->regs->cmdstatus, OHCI_HCR);
    val = 30;
    while (ohci_readl(ohci, &ohci->regs->cmdstatus) & OHCI_HCR) {
        if (--val == 0) return ETIMEDOUT;

        usleep(1);
    }

    ohci_writel(ohci, &ohci->regs->ed_bulkhead, 0);
    ohci_writel(ohci, &ohci->regs->ed_controlhead, 0);

    ohci_writel(ohci, &ohci->regs->hcca, ohci->hcca_phys);

    ohci_init_fminterval(ohci);

    ohci->hc_control &= OHCI_CTRL_RWC;
    ohci->hc_control |= OHCI_CONTROL_INIT | OHCI_USB_OPER;
    ohci_writel(ohci, &ohci->regs->control, ohci->hc_control);

    mask = OHCI_INTR_INIT;
    ohci_writel(ohci, &ohci->regs->intrstatus, ~0);
    ohci_writel(ohci, &ohci->regs->intrenable, mask);

    val = ohci_readl(ohci, &ohci->regs->roothub.a);
    val &= ~RH_A_NOCP;
    val |= ~RH_A_OCPM;
    ohci_writel(ohci, &ohci->regs->roothub.a, val);

    ohci_writel(ohci, &ohci->regs->roothub.status, RH_HS_LPSC);
    ohci_writel(ohci, &ohci->regs->roothub.b, (val & RH_A_NPS) ? 0 : RH_B_PPCM);

    (void)ohci_readl(ohci, &ohci->regs->control);

    usleep(((val >> 23) & 0x1fe) * 1000);

    return 0;
}

static int ohci_setup(struct usb_hcd* hcd)
{
    struct ohci_hcd* ohci = hcd_to_ohci(hcd);

    INIT_LIST_HEAD(&ohci->pending);
    INIT_LIST_HEAD(&ohci->eds_in_use);

    return ohci_init(ohci);
}

static int ohci_start(struct usb_hcd* hcd)
{
    struct ohci_hcd* ohci = hcd_to_ohci(hcd);

    return ohci_run(ohci);
}

static inline struct td* phys_to_td(struct ohci_hcd* ohci, phys_bytes td_phys)
{
    struct td* td;

    td_phys &= TD_MASK;
    td = ohci->td_hash[TD_HASH_FUNC(td_phys)];
    while (td && td->td_phys != td_phys)
        td = td->td_hash_next;
    return td;
}

static void ed_halted(struct ohci_hcd* ohci, struct td* td, int cc)
{
    struct urb* urb = td->urb;
    struct ohci_urb_priv* urb_priv = urb->hc_priv;
    struct ed* ed = td->ed;
    struct list_head* tmp = td->td_list.next;
    u32 toggle = ed->hw.head_p & cpu_to_le32(ED_C);

    ed->hw.info |= cpu_to_le32(ED_SKIP);
    wmb();
    ed->hw.head_p &= ~cpu_to_le32(ED_H);

    while (tmp != &ed->td_list) {
        struct td* next;

        next = list_entry(tmp, struct td, td_list);
        tmp = next->td_list.next;

        if (next->urb != urb) break;

        list_del(&next->td_list);
        urb_priv->finished++;
        ed->hw.head_p = next->hw.next_td | toggle;
    }
}

static void add_to_done_list(struct ohci_hcd* ohci, struct td* td)
{
    struct ed* ed;
    struct td *td2, *td_prev;

    if (td->done_next) return;

    ed = td->ed;
    td2 = td_prev = td;

    list_for_each_entry_continue_reverse(td2, &ed->td_list, td_list)
    {
        if (td2->done_next) break;
        td2->done_next = td_prev;
        td_prev = td2;
    }

    if (ohci->done_end)
        ohci->done_end->done_next = td_prev;
    else
        ohci->done_start = td_prev;

    ohci->done_end = td->done_next = td;
}

static void update_done_list(struct ohci_hcd* ohci)
{
    phys_bytes td_phys;
    struct td* td;

    td_phys = le32_to_cpu(ohci->hcca->done_head);
    ohci->hcca->done_head = 0;
    wmb();

    while (td_phys) {
        int cc;

        td = phys_to_td(ohci, td_phys);
        if (!td) {
            break;
        }

        td->hw.info |= cpu_to_le32(TD_DONE);
        cc = TD_CC_GET(le32_to_cpu(td->hw.info));

        if (cc != TD_CC_NOERROR && (td->ed->hw.head_p & cpu_to_le32(ED_H)))
            ed_halted(ohci, td, cc);

        td_phys = le32_to_cpu(td->hw.next_td);
        add_to_done_list(ohci, td);
    }
}

static int td_update_urb(struct ohci_hcd* ohci, struct urb* urb, struct td* td)
{
    u32 info = le32_to_cpu(td->hw.info);
    int cc;
    int status = EINPROGRESS;

    list_del(&td->td_list);

    if (info & TD_ISO) {
        /* TODO: isochronous */
    } else {
        int type = usb_pipetype(urb->pipe);
        u32 be = le32_to_cpu(td->hw.be);

        cc = TD_CC_GET(info);

        if (cc != TD_CC_NOERROR && cc < 0x0E) status = cc_to_error[cc];

        if ((type != PIPE_CONTROL || td->index != 0) && be != 0) {
            if (td->hw.cbp == 0)
                urb->actual_length += be - td->data_phys + 1;
            else
                urb->actual_length += le32_to_cpu(td->hw.cbp) - td->data_phys;
        }
    }

    return status;
}

static void finish_urb(struct ohci_hcd* ohci, struct urb* urb, int status)
{
    struct usb_hcd* hcd = ohci_to_hcd(ohci);
    struct usb_host_endpoint* ep = urb->ep;
    struct ohci_urb_priv* urb_priv;

restart:
    urb_free_priv(ohci, urb->hc_priv);
    urb->hc_priv = NULL;
    if (status == EINPROGRESS) status = 0;

    usb_hcd_unlink_urb_from_ep(hcd, urb);
    usb_hcd_giveback_urb(hcd, urb, status);

    if (!list_empty(&ep->urb_list)) {
        urb = list_first_entry(&ep->urb_list, struct urb, urb_list);
        urb_priv = urb->hc_priv;
        if (urb_priv->finished > urb_priv->length) {
            status = 0;
            goto restart;
        }
    }
}

static void start_ed_unlink(struct ohci_hcd* ohci, struct ed* ed)
{
    ed->hw.info |= cpu_to_le32(ED_DEQUEUE);
    ed_deschedule(ohci, ed);

    ed->ed_next = ohci->ed_rm_list;
    ed->ed_prev = NULL;
    ohci->ed_rm_list = ed;

    ohci_writel(ohci, &ohci->regs->intrstatus, OHCI_INTR_SF);
    ohci_writel(ohci, &ohci->regs->intrenable, OHCI_INTR_SF);
    ohci_readl(ohci, &ohci->regs->control);

    ed->tick = le32_to_cpu(ohci->hcca->frame_num) + 1;
}

static void td_complete(struct ohci_hcd* ohci, struct td* td)
{
    struct urb* urb = td->urb;
    struct ohci_urb_priv* urb_priv = urb->hc_priv;
    struct ed* ed = td->ed;
    int status;

    status = td_update_urb(ohci, urb, td);
    urb_priv->finished++;

    if (urb_priv->finished >= urb_priv->length) finish_urb(ohci, urb, status);

    if (list_empty(&ed->td_list)) {
        if (ed->state == ED_OPER) start_ed_unlink(ohci, ed);
    } else if ((ed->hw.info & cpu_to_le32(ED_SKIP | ED_DEQUEUE)) ==
               cpu_to_le32(ED_SKIP)) {
        td = list_entry(ed->td_list.next, struct td, td_list);

        if (!(td->hw.info & cpu_to_le32(TD_DONE))) {
            ed->hw.info &= ~cpu_to_le32(ED_SKIP);

            switch (ed->type) {
            case PIPE_CONTROL:
                ohci_writel(ohci, &ohci->regs->cmdstatus, OHCI_CLF);
                break;
            case PIPE_BULK:
                ohci_writel(ohci, &ohci->regs->cmdstatus, OHCI_BLF);
                break;
            }
        }
    }
}

static void ohci_process_td(struct ohci_hcd* ohci)
{
    struct td* td;

    while (ohci->done_start) {
        td = ohci->done_start;

        if (td == ohci->done_end)
            ohci->done_start = ohci->done_end = NULL;
        else
            ohci->done_start = td->done_next;

        td_complete(ohci, td);
    }
}

static void finish_unlinks(struct ohci_hcd* ohci)
{
    unsigned int tick = le32_to_cpu(ohci->hcca->frame_num);
    struct ed *ed, **last;

rescan_all:
    for (last = &ohci->ed_rm_list, ed = *last; ed != NULL; ed = *last) {
        struct td *td, *tmp;
        int completed, modified;
        u32* prev;

        if ((s16)((s16)tick - (s16)ed->tick) < 0) {
        skip_ed:
            last = &ed->ed_next;
            continue;
        }

        if (!list_empty(&ed->td_list)) {
            struct td* td;
            u32 head;

            td = list_first_entry(&ed->td_list, struct td, td_list);

            head = le32_to_cpu(ed->hw.head_p) & TD_MASK;
            if (td->td_phys != head) goto skip_ed;

            if (td->done_next) goto skip_ed;
        }

        ed->hw.head_p &= ~cpu_to_le32(ED_H);
        ed->hw.next_ed = 0;
        wmb();
        ed->hw.info &= ~cpu_to_le32(ED_SKIP | ED_DEQUEUE);

        *last = ed->ed_next;
        ed->ed_next = NULL;
        modified = FALSE;

    rescan_this:
        completed = FALSE;
        prev = &ed->hw.head_p;
        list_for_each_entry_safe(td, tmp, &ed->td_list, td_list)
        {
            struct urb* urb;
            struct ohci_urb_priv* urb_priv;
            u32 savebits;
            u32 td_info;

            urb = td->urb;
            urb_priv = td->urb->hc_priv;

            if (!urb->unlinked) {
                prev = &td->hw.next_td;
                continue;
            }

            savebits = *prev & ~cpu_to_le32(TD_MASK);
            *prev = td->hw.next_td | savebits;

            td_info = le32_to_cpu(td->hw.info);
            if ((td_info & TD_T) == TD_T_DATA0)
                ed->hw.head_p &= ~cpu_to_le32(ED_C);
            else if ((td_info & TD_T) == TD_T_DATA1)
                ed->hw.head_p |= cpu_to_le32(ED_C);

            td_update_urb(ohci, urb, td);
            urb_priv->finished++;

            if (urb_priv->finished >= urb_priv->length) {
                modified = completed = TRUE;
                finish_urb(ohci, urb, 0);
            }
        }
        if (completed && !list_empty(&ed->td_list)) goto rescan_this;

        if (list_empty(&ed->td_list)) {
            ed->state = ED_IDLE;
            list_del(&ed->in_use_list);
        } else {
            ed_schedule(ohci, ed);
        }

        if (modified) goto rescan_all;
    }

    if (!ohci->ed_rm_list) {
        u32 command = 0, control = 0;

        if (ohci->ed_controltail) {
            command |= OHCI_CLF;
            if (!(ohci->hc_control & OHCI_CTRL_CLE)) {
                control |= OHCI_CTRL_CLE;
                ohci_writel(ohci, &ohci->regs->ed_controlcurrent, 0);
            }
        }
        if (ohci->ed_bulktail) {
            command |= OHCI_BLF;
            if (!(ohci->hc_control & OHCI_CTRL_BLE)) {
                control |= OHCI_CTRL_BLE;
                ohci_writel(ohci, &ohci->regs->ed_bulkcurrent, 0);
            }
        }

        if (control) {
            ohci->hc_control |= control;
            ohci_writel(ohci, &ohci->regs->control, ohci->hc_control);
        }
        if (command) {
            ohci_writel(ohci, &ohci->regs->cmdstatus, command);
        }
    }
}

static void ohci_process(struct ohci_hcd* ohci)
{
    ohci_process_td(ohci);

    if (ohci->ed_rm_list) finish_unlinks(ohci);
}

static void ohci_irq(struct usb_hcd* hcd)
{
    struct ohci_hcd* ohci = hcd_to_ohci(hcd);
    struct ohci_regs* regs = ohci->regs;
    u32 ints;

    ints = ohci_readl(ohci, &regs->intrstatus);

again:
    ints &= ohci_readl(ohci, &regs->intrenable);

    if (ints == 0) goto out;

    if (ints & OHCI_INTR_RHSC) {
        ohci_writel(ohci, &regs->intrstatus, OHCI_INTR_RD | OHCI_INTR_RHSC);

        ohci_writel(ohci, &regs->intrdisable, OHCI_INTR_RHSC);
        usb_hcd_poll_rh_status(hcd);
    }

    if (ints & OHCI_INTR_WDH) update_done_list(ohci);

    ohci_process(ohci);
    if ((ints & OHCI_INTR_SF) != 0 && !ohci->ed_rm_list)
        ohci_writel(ohci, &regs->intrdisable, OHCI_INTR_SF);

    ohci_writel(ohci, &regs->intrstatus, ints);
    ohci_writel(ohci, &regs->intrenable, OHCI_INTR_MIE);
    ohci_readl(ohci, &ohci->regs->control);

    ints = ohci_readl(ohci, &regs->intrstatus);
    if (ints && (ints & ohci_readl(ohci, &regs->intrenable))) goto again;

out:
    irq_enable(&hcd->irq_hook);
}

static int ohci_hub_status_data(struct usb_hcd* hcd, char* buf)
{
    struct ohci_hcd* ohci = hcd_to_ohci(hcd);
    int length = 1;
    int changed = FALSE;
    int i;

    if (ohci_readl(ohci, &ohci->regs->roothub.status) &
        (RH_HS_LPSC | RH_HS_OCIC))
        buf[0] = changed = 1;
    else
        buf[0] = 0;

    if (ohci->num_ports > 7) {
        length++;
        buf[1] = 0;
    }

    for (i = 0; i < ohci->num_ports; i++) {
        u32 status = ohci_readl(ohci, &ohci->regs->roothub.portstatus[i]);

        if (status &
            (RH_PS_CSC | RH_PS_PESC | RH_PS_PSSC | RH_PS_OCIC | RH_PS_PRSC)) {
            changed = 1;
            if (i < 7)
                buf[0] |= 1 << (i + 1);
            else
                buf[1] |= 1 << (i - 7);
        }
    }

    ohci_writel(ohci, &ohci->regs->intrenable, OHCI_INTR_RHSC);

    return changed ? length : 0;
}

#define PORT_RESET_MSEC    50
#define PORT_RESET_HW_MSEC 10

static inline int root_port_reset(struct ohci_hcd* ohci, unsigned port)
{
    u32* portstatus = &ohci->regs->roothub.portstatus[port];
    u32 val;
    u16 now = ohci_readl(ohci, &ohci->regs->fmnumber);
    u16 reset_done = now + PORT_RESET_MSEC;
    int limit_1 =
        (PORT_RESET_MSEC + PORT_RESET_HW_MSEC - 1) / PORT_RESET_HW_MSEC;

    do {
        int limit_2 = PORT_RESET_HW_MSEC * 2;

        while (limit_2 >= 0) {
            val = ohci_readl(ohci, portstatus);

            if (!(val & RH_PS_PRS)) break;
            usleep(500);
        }

        if (limit_2 < 0) break;

        if (!(val & RH_PS_CCS)) break;
        if (val & RH_PS_PRSC) ohci_writel(ohci, portstatus, RH_PS_PRSC);

        ohci_writel(ohci, portstatus, RH_PS_PRS);
        usleep(PORT_RESET_HW_MSEC * 1000);
        now = ohci_readl(ohci, &ohci->regs->fmnumber);
    } while ((s16)((s16)now - (s16)reset_done) < 0 && --limit_1);

    return 0;
}

int ohci_hub_control(struct usb_hcd* hcd, u16 typeReq, u16 wValue, u16 wIndex,
                     char* buf, u16 wLength)
{
    struct ohci_hcd* ohci = hcd_to_ohci(hcd);
    int ports = ohci->num_ports;
    u32 val;
    int retval = 0;

    switch (typeReq) {
    case ClearPortFeature:
        if (!wIndex || wIndex > ports) goto error;
        wIndex--;

        switch (wValue) {
        case USB_PORT_FEAT_ENABLE:
            val = RH_PS_CCS;
            break;
        case USB_PORT_FEAT_C_ENABLE:
            val = RH_PS_PESC;
            break;
        case USB_PORT_FEAT_SUSPEND:
            val = RH_PS_POCI;
            break;
        case USB_PORT_FEAT_C_SUSPEND:
            val = RH_PS_PSSC;
            break;
        case USB_PORT_FEAT_POWER:
            val = RH_PS_LSDA;
            break;
        case USB_PORT_FEAT_C_CONNECTION:
            val = RH_PS_CSC;
            break;
        case USB_PORT_FEAT_C_OVER_CURRENT:
            val = RH_PS_OCIC;
            break;
        case USB_PORT_FEAT_C_RESET:
            val = RH_PS_PRSC;
            break;
        default:
            goto error;
        }
        ohci_writel(ohci, &ohci->regs->roothub.portstatus[wIndex], val);
        break;

    case GetPortStatus:
        if (!wIndex || wIndex > ports) goto error;
        wIndex--;
        val = ohci_readl(ohci, &ohci->regs->roothub.portstatus[wIndex]);
        *(u32*)buf = val;
        break;

    case SetPortFeature:
        if (!wIndex || wIndex > ports) goto error;
        wIndex--;
        switch (wValue) {
        case USB_PORT_FEAT_SUSPEND:
            ohci_writel(ohci, &ohci->regs->roothub.portstatus[wIndex],
                        RH_PS_PSS);
            break;
        case USB_PORT_FEAT_POWER:
            ohci_writel(ohci, &ohci->regs->roothub.portstatus[wIndex],
                        RH_PS_PPS);
            break;
        case USB_PORT_FEAT_RESET:
            retval = -root_port_reset(ohci, wIndex);
            break;
        default:
            goto error;
        }
        break;

    error:
        retval = -EPIPE;
    }

    return retval;
}

static struct ed* ed_alloc(struct ohci_hcd* ohci)
{
    struct ed* ed;
    int retval;

    retval = posix_memalign((void**)&ed, 16, sizeof(*ed));
    if (retval) return NULL;

    memset(ed, 0, sizeof(*ed));

    retval = umap(SELF, UMT_VADDR, (vir_bytes)ed, sizeof(*ed), &ed->phys);
    if (retval) {
        free(ed);
        return NULL;
    }

    INIT_LIST_HEAD(&ed->td_list);
    return ed;
}

static void ed_free(struct ohci_hcd* ohci, struct ed* ed) { free(ed); }

static struct td* td_alloc(struct ohci_hcd* ohci)
{
    struct td* td;
    int retval;

    retval = posix_memalign((void**)&td, 32, sizeof(*td));
    if (retval) return NULL;

    memset(td, 0, sizeof(*td));

    retval = umap(SELF, UMT_VADDR, (vir_bytes)td, sizeof(*td), &td->td_phys);
    if (retval) {
        free(td);
        return NULL;
    }

    td->hw.next_td = cpu_to_le32(td->td_phys);
    return td;
}

static void td_free(struct ohci_hcd* ohci, struct td* td) { free(td); }

static struct ed* ed_get(struct ohci_hcd* ohci, struct usb_host_endpoint* ep,
                         struct usb_device* udev, unsigned int pipe,
                         int interval)
{
    struct ed* ed;

    ed = ep->hcpriv;
    if (!ed) {
        struct td* td;
        int is_out;
        u32 info;

        ed = ed_alloc(ohci);
        if (!ed) return NULL;

        td = td_alloc(ohci);
        if (!td) {
            ed_free(ohci, ed);
            return NULL;
        }

        ed->tail_td = td;
        ed->hw.tail_p = cpu_to_le32(td->td_phys);
        ed->hw.head_p = ed->hw.tail_p;

        is_out = !(ep->desc.bEndpointAddress & USB_DIR_IN);

        info = usb_pipedevice(pipe);
        ed->type = usb_pipetype(pipe);

        info |= (ep->desc.bEndpointAddress & ~USB_DIR_IN) << 7;
        info |= usb_endpoint_maxp(&ep->desc) << 16;
        if (udev->speed == USB_SPEED_LOW) info |= ED_LOWSPEED;

        if (ed->type != PIPE_CONTROL) {
            info |= is_out ? ED_OUT : ED_IN;
            if (ed->type != PIPE_BULK) {
                if (ed->type == PIPE_ISOCHRONOUS)
                    info |= ED_ISO;
                else if (interval > 32)
                    interval = 32;
                ed->interval = interval;
            }
        }

        ed->hw.info = cpu_to_le32(info);
        ep->hcpriv = ed;
    }

    return ed;
}

static void urb_free_priv(struct ohci_hcd* ohci, struct ohci_urb_priv* urb_priv)
{
    int last = urb_priv->length - 1;

    if (last >= 0) {
        int i;
        struct td* td;

        for (i = 0; i <= last; i++) {
            td = urb_priv->td[i];
            if (td) td_free(ohci, td);
        }
    }

    list_del(&urb_priv->pending);
    free(urb_priv);
}

static int ed_schedule(struct ohci_hcd* ohci, struct ed* ed)
{
    ed->ed_prev = NULL;
    ed->ed_next = NULL;
    ed->hw.next_ed = 0;
    wmb();

    switch (ed->type) {
    case PIPE_CONTROL:
        if (!ohci->ed_controltail) {
            ohci_writel(ohci, &ohci->regs->ed_controlhead, ed->phys);
        } else {
            ohci->ed_controltail->ed_next = ed;
            ohci->ed_controltail->hw.next_ed = cpu_to_le32(ed->phys);
        }
        ed->ed_prev = ohci->ed_controltail;
        if (!ohci->ed_controltail && !ohci->ed_rm_list) {
            wmb();
            ohci->hc_control |= OHCI_CTRL_CLE;
            ohci_writel(ohci, &ohci->regs->ed_controlcurrent, 0);
            ohci_writel(ohci, &ohci->regs->control, ohci->hc_control);
        }
        ohci->ed_controltail = ed;
        break;
    }

    ed->state = ED_OPER;

    return 0;
}

static void ed_deschedule(struct ohci_hcd* ohci, struct ed* ed)
{
    ed->hw.info |= cpu_to_le32(ED_SKIP);
    wmb();
    ed->state = ED_UNLINK;

    switch (ed->type) {
    case PIPE_CONTROL:
        if (ed->ed_prev == NULL) {
            if (!ed->hw.next_ed) {
                ohci->hc_control &= ~OHCI_CTRL_CLE;
                ohci_writel(ohci, &ohci->regs->control, ohci->hc_control);
            } else
                ohci_writel(ohci, &ohci->regs->ed_controlhead,
                            le32_to_cpu(ed->hw.next_ed));
        } else {
            ed->ed_prev->ed_next = ed->ed_next;
            ed->ed_prev->hw.next_ed = ed->hw.next_ed;
        }

        if (ohci->ed_controltail == ed) {
            ohci->ed_controltail = ed->ed_prev;
            if (ohci->ed_controltail) ohci->ed_controltail->ed_next = NULL;
        } else if (ed->ed_next) {
            ed->ed_next->ed_prev = ed->ed_prev;
        }
        break;
    }
}

static void td_fill(struct ohci_hcd* ohci, u32 info, phys_bytes data_phys,
                    int len, struct urb* urb, int index)
{
    struct td *td, *td_pt;
    struct ohci_urb_priv* urb_priv = urb->hc_priv;
    int is_iso = info & TD_ISO;
    int hash;

    if (index != (urb_priv->length - 1) ||
        (urb->transfer_flags & URB_NO_INTERRUPT))
        info |= TD_DI_SET(6);

    td_pt = urb_priv->td[index];

    td = urb_priv->td[index] = urb_priv->ed->tail_td;
    urb_priv->ed->tail_td = td_pt;

    td->ed = urb_priv->ed;
    td->index = index;
    td->urb = urb;
    td->data_phys = data_phys;
    if (!len) data_phys = 0;

    td->hw.info = cpu_to_le32(info);
    if (is_iso) {
        /* TODO: isochronous */
    } else
        td->hw.cbp = cpu_to_le32(data_phys);

    if (data_phys)
        td->hw.be = cpu_to_le32(data_phys + len - 1);
    else
        td->hw.be = 0;
    td->hw.next_td = cpu_to_le32(td_pt->td_phys);

    list_add_tail(&td->td_list, &td->ed->td_list);

    hash = TD_HASH_FUNC(td->td_phys);
    td->td_hash_next = ohci->td_hash[hash];
    ohci->td_hash[hash] = td;

    wmb();
    td->ed->hw.tail_p = td->hw.next_td;
}

static void td_submit_urb(struct ohci_hcd* ohci, struct urb* urb)
{
    struct ohci_urb_priv* urb_priv = urb->hc_priv;
    phys_bytes data_phys;
    int data_len = urb->transfer_buffer_length;
    int cnt = 0;
    u32 info = 0;
    int is_out = usb_pipeout(urb->pipe);

    if (!usb_gettoggle(urb->dev, usb_pipeendpoint(urb->pipe), is_out)) {
        usb_settoggle(urb->dev, usb_pipeendpoint(urb->pipe), is_out, 1);
        urb_priv->ed->hw.head_p &= ~cpu_to_le32(ED_C);
    }

    if (data_len)
        data_phys = urb->transfer_phys;
    else
        data_phys = 0;

    switch (urb_priv->ed->type) {
    case PIPE_CONTROL:
        info = TD_CC | TD_DP_SETUP | TD_T_DATA0;
        td_fill(ohci, info, urb->setup_phys, 8, urb, cnt++);

        if (data_len > 0) {
            info = TD_CC | TD_R | TD_T_DATA1;
            info |= is_out ? TD_DP_OUT : TD_DP_IN;
            td_fill(ohci, info, data_phys, data_len, urb, cnt++);
        }

        info = (is_out || data_len == 0) ? TD_CC | TD_DP_IN | TD_T_DATA1
                                         : TD_CC | TD_DP_OUT | TD_T_DATA1;
        td_fill(ohci, info, data_phys, 0, urb, cnt++);
        wmb();

        ohci_writel(ohci, &ohci->regs->cmdstatus, OHCI_CLF);
        break;
    }
}

static int ohci_urb_enqueue(struct usb_hcd* hcd, struct urb* urb)
{
    struct ohci_hcd* ohci = hcd_to_ohci(hcd);
    unsigned int pipe = urb->pipe;
    int td_count = 0;
    struct ohci_urb_priv* urb_priv;
    size_t urb_priv_size;
    struct ed* ed;
    int i, retval;

    ed = ed_get(ohci, urb->ep, urb->dev, pipe, urb->interval);
    if (!ed) return ENOMEM;

    switch (ed->type) {
    case PIPE_ISOCHRONOUS:
        td_count = urb->num_packets;
        break;

    case PIPE_CONTROL:
        if (urb->transfer_buffer_length > 4096) return EMSGSIZE;
        td_count = 2;

    default:
        td_count += (urb->transfer_buffer_length + 4096 - 1) / 4096;

        if (!td_count) td_count++;
        break;
    }

    urb_priv_size =
        sizeof(struct ohci_urb_priv) + td_count * sizeof(urb_priv->td[0]);
    urb_priv = malloc(urb_priv_size);
    if (!urb_priv) return ENOMEM;

    memset(urb_priv, 0, urb_priv_size);
    INIT_LIST_HEAD(&urb_priv->pending);
    urb_priv->length = td_count;
    urb_priv->ed = ed;

    for (i = 0; i < td_count; i++) {
        urb_priv->td[i] = td_alloc(ohci);
        if (!urb_priv->td[i]) {
            urb_priv->length = i;
            urb_free_priv(ohci, urb_priv);
            return ENOMEM;
        }
    }

    retval = usb_hcd_link_urb_to_ep(hcd, urb);
    if (retval) goto out;

    if (ed->state == ED_IDLE) {
        retval = ed_schedule(ohci, ed);
        if (retval) {
            usb_hcd_unlink_urb_from_ep(hcd, urb);
            goto out;
        }

        list_add(&ed->in_use_list, &ohci->eds_in_use);
    }

    urb->hc_priv = urb_priv;
    td_submit_urb(ohci, urb);

out:
    if (retval) urb_free_priv(ohci, urb_priv);

    return retval;
}

static void ohci_disable_endpoint(struct usb_hcd* hcd,
                                  struct usb_host_endpoint* ep)
{
    struct ohci_hcd* ohci = hcd_to_ohci(hcd);
    struct ed* ed = ep->hcpriv;
    int limit = 1000;

    if (!ed) return;

rescan:
    switch (ed->state) {
    case ED_UNLINK:
        ohci_irq(hcd);

        if (limit-- == 0) {
            ed->state = ED_IDLE;
            ohci_process(ohci);
        }

        usleep(1000);
        goto rescan;

    case ED_IDLE:
        if (list_empty(&ed->td_list)) {
            td_free(ohci, ed->tail_td);
            ed_free(ohci, ed);
            break;
        }

    default:
        td_free(ohci, ed->tail_td);
        break;
    }

    ep->hcpriv = NULL;
}

static const struct hc_driver ohci_hc_driver = {
    .description = hcd_name,
    .product_desc = "OHCI Host Controller",
    .hcd_priv_size = sizeof(struct ohci_hcd),
    .flags = HCD_USB11 | HCD_MEMORY,

    .irq = ohci_irq,

    .setup = ohci_setup,
    .start = ohci_start,

    .urb_enqueue = ohci_urb_enqueue,

    .disable_endpoint = ohci_disable_endpoint,

    .hub_status_data = ohci_hub_status_data,
    .hub_control = ohci_hub_control,
};

void ohci_init_driver(struct hc_driver* driver) { *driver = ohci_hc_driver; }
