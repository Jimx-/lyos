#ifndef _USBD_OHCI_H_
#define _USBD_OHCI_H_

#include <lyos/types.h>
#include <lyos/const.h>

#include "hcd.h"

struct ed {
    struct {
        u32 info;
#define ED_DEQUEUE  (1 << 27)
#define ED_ISO      (1 << 15)
#define ED_SKIP     (1 << 14)
#define ED_LOWSPEED (1 << 13)
#define ED_OUT      (0x01 << 11)
#define ED_IN       (0x02 << 11)
        u32 tail_p;
        u32 head_p;
#define ED_C (0x02) /* toggle carry */
#define ED_H (0x01) /* halted */
        u32 next_ed;
    } hw;

    struct td* tail_td;
    phys_bytes phys;

    struct ed* ed_prev;
    struct ed* ed_next;
    struct list_head td_list;
    struct list_head in_use_list;

    u8 state;
#define ED_IDLE   0x00
#define ED_UNLINK 0x01
#define ED_OPER   0x02

    u8 type;
    u16 interval;

    u16 tick;
} __attribute__((aligned(16)));

struct td {
    struct {
        u32 info;
#define TD_CC           0xf0000000 /* condition code */
#define TD_CC_GET(td_p) ((td_p >> 28) & 0x0f)

#define TD_DI        0x00E00000 /* delay interrupt */
#define TD_DI_SET(X) (((X) & 0x07) << 21)
#define TD_DONE      0x00020000 /* writeback of donelist */
#define TD_ISO       0x00010000 /* copy of ED_ISO */

#define TD_EC       0x0C000000 /* error count */
#define TD_T        0x03000000 /* data toggle state */
#define TD_T_DATA0  0x02000000 /* DATA0 */
#define TD_T_DATA1  0x03000000 /* DATA1 */
#define TD_T_TOGGLE 0x00000000 /* uses ED_C */
#define TD_DP       0x00180000 /* direction/pid */
#define TD_DP_SETUP 0x00000000 /* SETUP pid */
#define TD_DP_IN    0x00100000 /* IN pid */
#define TD_DP_OUT   0x00080000 /* OUT pid */
#define TD_R        0x00040000 /* buffer rounding */

        u32 cbp;
        u32 next_td;
        u32 be;

#define MAXPSW 2
        u16 psw[MAXPSW];
    } hw;

    u8 index;
    struct ed* ed;
    struct td* td_hash_next;
    struct td* done_next;
    struct urb* urb;

    phys_bytes td_phys;
    phys_bytes data_phys;

    struct list_head td_list;
} __attribute__((aligned(32)));

#define TD_MASK ((u32)~0x1f)

#define TD_CC_NOERROR     0x00
#define TD_CC_CRC         0x01
#define TD_CC_BITSTUFFING 0x02
#define TD_CC_DATATOGGLEM 0x03
#define TD_CC_STALL       0x04
#define TD_DEVNOTRESP     0x05
#define TD_PIDCHECKFAIL   0x06
#define TD_UNEXPECTEDPID  0x07
#define TD_DATAOVERRUN    0x08
#define TD_DATAUNDERRUN   0x09
#define TD_BUFFEROVERRUN  0x0C
#define TD_BUFFERUNDERRUN 0x0D
#define TD_NOTACCESSED    0x0F

struct ohci_regs {
    u32 revision;
    u32 control;
    u32 cmdstatus;
    u32 intrstatus;
    u32 intrenable;
    u32 intrdisable;
    u32 hcca;
    u32 ed_periodcurrent;
    u32 ed_controlhead;
    u32 ed_controlcurrent;
    u32 ed_bulkhead;
    u32 ed_bulkcurrent;
    u32 donehead;
    u32 fminterval;
    u32 fmremaining;
    u32 fmnumber;
    u32 periodicstart;
    u32 lsthresh;

    struct ohci_roothub_regs {
        u32 a;
        u32 b;
        u32 status;
#define MAX_ROOT_PORTS 15
        u32 portstatus[MAX_ROOT_PORTS];
    } roothub;
} __attribute__((aligned(32)));

/* HcControl */
#define OHCI_CTRL_CBSR (3 << 0)  /* control/bulk service ratio */
#define OHCI_CTRL_PLE  (1 << 2)  /* periodic list enable */
#define OHCI_CTRL_IE   (1 << 3)  /* isochronous enable */
#define OHCI_CTRL_CLE  (1 << 4)  /* control list enable */
#define OHCI_CTRL_BLE  (1 << 5)  /* bulk list enable */
#define OHCI_CTRL_HCFS (3 << 6)  /* host controller functional state */
#define OHCI_CTRL_IR   (1 << 8)  /* interrupt routing */
#define OHCI_CTRL_RWC  (1 << 9)  /* remote wakeup connected */
#define OHCI_CTRL_RWE  (1 << 10) /* remote wakeup enable */

#define OHCI_USB_RESET   (0 << 6)
#define OHCI_USB_RESUME  (1 << 6)
#define OHCI_USB_OPER    (2 << 6)
#define OHCI_USB_SUSPEND (3 << 6)

/* HcCommandStatus */
#define OHCI_HCR (1 << 0)  /* host controller reset */
#define OHCI_CLF (1 << 1)  /* control list filled */
#define OHCI_BLF (1 << 2)  /* bulk list filled */
#define OHCI_OCR (1 << 3)  /* ownership change request */
#define OHCI_SOC (3 << 16) /* scheduling overrun count */

/* HcInterruptStatus/HcInterruptEnable/HcInterruptDisable */
#define OHCI_INTR_SO   (1 << 0)  /* scheduling overrun */
#define OHCI_INTR_WDH  (1 << 1)  /* hcdonehead writeback */
#define OHCI_INTR_SF   (1 << 2)  /* start frame */
#define OHCI_INTR_RD   (1 << 3)  /* resume detect */
#define OHCI_INTR_UE   (1 << 4)  /* unrecoverable error */
#define OHCI_INTR_FNO  (1 << 5)  /* frame number overflow */
#define OHCI_INTR_RHSC (1 << 6)  /* root hub status change */
#define OHCI_INTR_OC   (1 << 30) /* ownership change */
#define OHCI_INTR_MIE  (1 << 31) /* master interrupt enable */

/* HcFmInterval */
#define FI       0x2edf
#define FSMP(fi) (0x7fff & ((6 * ((fi) - 210)) / 7))
#define FIT      (1 << 31)

/* HcRhDescriptorA */
#define RH_A_NDP    (0xff << 0)  /* number of downstream ports */
#define RH_A_PSM    (1 << 8)     /* power switching mode */
#define RH_A_NPS    (1 << 9)     /* no power switching */
#define RH_A_DT     (1 << 10)    /* device type (mbz) */
#define RH_A_OCPM   (1 << 11)    /* over current protection mode */
#define RH_A_NOCP   (1 << 12)    /* no over current protection */
#define RH_A_POTPGT (0xff << 24) /* power on to power good time */

/* HcRhDescriptorB */
#define RH_B_DR   0x0000ffff /* device removable flags */
#define RH_B_PPCM 0xffff0000 /* port power control mask */

/* HcRhStatus */
#define RH_HS_LPS  0x00000001 /* local power status */
#define RH_HS_OCI  0x00000002 /* over current indicator */
#define RH_HS_DRWE 0x00008000 /* device remote wakeup enable */
#define RH_HS_LPSC 0x00010000 /* local power status change */
#define RH_HS_OCIC 0x00020000 /* over current indicator change */
#define RH_HS_CRWE 0x80000000 /* clear remote wakeup enable */

/* HcRhPortStatus */
#define RH_PS_CCS  0x00000001 /* current connect status */
#define RH_PS_PES  0x00000002 /* port enable status*/
#define RH_PS_PSS  0x00000004 /* port suspend status */
#define RH_PS_POCI 0x00000008 /* port over current indicator */
#define RH_PS_PRS  0x00000010 /* port reset status */
#define RH_PS_PPS  0x00000100 /* port power status */
#define RH_PS_LSDA 0x00000200 /* low speed device attached */
#define RH_PS_CSC  0x00010000 /* connect status change */
#define RH_PS_PESC 0x00020000 /* port enable status change */
#define RH_PS_PSSC 0x00040000 /* port suspend status change */
#define RH_PS_OCIC 0x00080000 /* over current indicator change */
#define RH_PS_PRSC 0x00100000 /* port reset status change */

struct ohci_hcca {
#define NUM_INTS 32
    u32 int_table[NUM_INTS]; /* interrupt table */
    u32 frame_num;           /* current frame number */
    u32 done_head;           /* done head writeback */
    u8 _reserved[120];
} __attribute__((aligned(256)));

struct ohci_urb_priv {
    struct ed* ed;
    u16 length;
    u16 finished;
    struct list_head pending;
    struct td* td[0];
};

#define TD_HASH_SIZE          64
#define TD_HASH_FUNC(td_phys) ((td_phys ^ (td_phys >> 6)) % TD_HASH_SIZE)

struct ohci_hcd {
    struct ohci_regs* regs;

    struct ohci_hcca* hcca;
    phys_bytes hcca_phys;

    struct ed* ed_rm_list;

    struct ed* ed_bulktail;
    struct ed* ed_controltail;
    struct ed* periodic[NUM_INTS];

    struct td* td_hash[TD_HASH_SIZE];
    struct td* done_start;
    struct td* done_end;
    struct list_head pending;
    struct list_head eds_in_use;

    int num_ports;
    u32 hc_control;
    u32 fminterval;
};

static inline struct ohci_hcd* hcd_to_ohci(struct usb_hcd* hcd)
{
    return (struct ohci_hcd*)(hcd->hcd_priv);
}
static inline struct usb_hcd* ohci_to_hcd(const struct ohci_hcd* ohci)
{
    return list_entry((void*)ohci, struct usb_hcd, hcd_priv);
}

static inline u32 ohci_readl(const struct ohci_hcd* ohci, u32* reg)
{
    u32 val;

    val = readl(reg);
    return val;
}

static inline void ohci_writel(const struct ohci_hcd* ohci, u32* reg, u32 value)
{
    writel(reg, value);
}

static inline void ohci_init_fminterval(const struct ohci_hcd* ohci)
{
    u32 fi = ohci->fminterval & 0x03fff;
    u32 fit = ohci_readl(ohci, &ohci->regs->fminterval) & FIT;

    ohci_writel(ohci, &ohci->regs->fminterval, (fit ^ FIT) | ohci->fminterval);
    ohci_writel(ohci, &ohci->regs->periodicstart, ((9 * fi) / 10) & 0x3fff);
}

void ohci_init_driver(struct hc_driver* driver);

#if CONFIG_USB_OHCI_HCD_PCI
int ohci_pci_init(void);
int ohci_pci_probe(int devind);
#endif

#endif
