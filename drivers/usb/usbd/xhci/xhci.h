#ifndef _USBD_XHCI_H_
#define _USBD_XHCI_H_

#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/list.h>
#include <lyos/dmapool.h>
#include <asm/io.h>
#include <libasyncdriver/libasyncdriver.h>

#include "hcd.h"

#ifndef readq
static inline u64 readq(const void* addr)
{
    u32 lo = readl(addr);
    u32 hi = readl((const void*)((const char*)addr + 4));
    return ((u64)hi << 32) | lo;
}
#endif

#ifndef writeq
static inline void writeq(void* addr, u64 val)
{
    writel(addr, (u32)(val & 0xffffffff));
    writel((void*)((char*)addr + 4), (u32)(val >> 32));
}
#endif

/*
 * xHCI (eXtensible Host Controller Interface) register and data structure
 * definitions. Based on the xHCI Specification Revision 1.2.
 */

/* =========================================================================
 * Capability Registers (read-only, at MMIO base)
 * ========================================================================= */
struct xhci_cap_regs {
    u8 hc_length; /* bits 0-7: capability register length */
    u8 reserved;
    u16 hciversion; /* interface version number (BCD) */
    u32 hcsparams1; /* structural parameters 1 */
    u32 hcsparams2; /* structural parameters 2 */
    u32 hcsparams3; /* structural parameters 3 */
    u32 hccparams1; /* capability parameters 1 */
    u32 dboff;      /* doorbell offset */
    u32 rtsoff;     /* runtime register space offset */
    u32 hccparams2; /* capability parameters 2 */
};

/* HCSPARAMS1 bit fields */
#define HCS_MAX_SLOTS(p) (((p) >> 0) & 0xff)
#define HCS_MAX_INTRS(p) (((p) >> 8) & 0x3ff)
#define HCS_MAX_PORTS(p) (((p) >> 24) & 0xff)

/* HCSPARAMS2 bit fields */
#define HCS_IST(p)              (((p) >> 0) & 0xf)
#define HCS_ERST_MAX(p)         (((p) >> 4) & 0xf)
#define HCS_MAX_SCRATCH_BUFS(p) (((p) >> 27) & 0x1f)

/* HCSPARAMS3 bit fields */
#define HCS_U1_DEV_EXIT_LAT(p) (((p) >> 0) & 0xff)
#define HCS_U2_DEV_EXIT_LAT(p) (((p) >> 16) & 0xffff)

/* HCCPARAMS1 bit fields */
#define HCC_EXT_CAPS(p)       (((p) >> 16) & 0xffff)
#define HCC_64BIT_ADDR(p)     ((p) & 0x00000001)
#define HCC_BANDWIDTH_NEG(p)  ((p) & 0x00000002)
#define HCC_64BYTE_CONTEXT(p) ((p) & 0x00000004)
#define HCC_PPC(p)            ((p) & 0x00000008) /* port power control */
#define HCS_INDIRECT(p)       ((p) & 0x00000010)
#define HCC_LIGHT_RESET(p)    ((p) & 0x00000020)
#define HCC_LTC(p)            ((p) & 0x00000040)
#define HCC_NTT(p)            ((p) & 0x00000080)
#define HCC_SEC_SIDEBAND(p)   ((p) & 0x00000100)
#define HCC_PARSE_ALL(p)      ((p) & 0x00000200)

/* HCCPARAMS2 bit fields */
#define HCC_U3_ENTRY(p) ((p) & 0x00000001)
#define HCC_CMC(p)      ((p) & 0x00000002)
#define HCC_FSC(p)      ((p) & 0x00000004)
#define HCC_CTC(p)      ((p) & 0x00000008)
#define HCC_LEC(p)      ((p) & 0x00000010)
#define HCC_CIC(p)      ((p) & 0x00000020)

/* =========================================================================
 * Operational Registers (at MMIO base + HC_LENGTH)
 * ========================================================================= */
struct xhci_op_regs {
    u32 usbcmd;       /* USB command (0x00) */
    u32 usbsts;       /* USB status (0x04) */
    u32 pagesize;     /* page size (0x08, read-only) */
    u32 reserved1;    /* 0x0C */
    u32 reserved2;    /* 0x10 */
    u32 dnctrl;       /* device notification control (0x14) */
    u64 crcr;         /* command ring control (0x18) */
    u32 reserved3[4]; /* 0x20 - 0x2F */
    u64 dcbaap;       /* device context base address array pointer (0x30) */
    u32 config;       /* configure (0x38) */
    u32 reserved4[(0x400 - 0x3C) / 4]; /* pad to port offset 0x400 */
    u32 portsc_base[0];                /* port status and control (0x400) */
};

/* USBCMD bit fields */
#define CMD_RUN       (1 << 0)  /* run/stop */
#define CMD_RESET     (1 << 1)  /* host controller reset */
#define CMD_EIE       (1 << 2)  /* interrupter enable */
#define CMD_HSEE      (1 << 3)  /* host system error enable */
#define CMD_LHCRST    (1 << 7)  /* light host controller reset */
#define CMD_CSS       (1 << 8)  /* controller save state */
#define CMD_CRS       (1 << 9)  /* controller restore state */
#define CMD_EWE       (1 << 10) /* enable wrap event */
#define CMD_PM_INDEX  (1 << 11) /* power management index */
#define CMD_RESERVED2 (1 << 12)
#define CMD_ETE       (1 << 14) /* extended TBC enable */
#define CMD_TSEL_EN   (1 << 15) /* transfer type select enable */

/* USBSTS bit fields */
#define STS_HCH  (1 << 0)  /* host controller halted */
#define STS_HSE  (1 << 2)  /* host system error */
#define STS_EINT (1 << 3)  /* event interrupt */
#define STS_PCD  (1 << 4)  /* port change detected */
#define STS_SSS  (1 << 8)  /* save state status */
#define STS_RSS  (1 << 9)  /* restore state status */
#define STS_SRE  (1 << 10) /* save/restore error */
#define STS_CNR  (1 << 11) /* controller not ready */
#define STS_HCE  (1 << 12) /* host controller error */

/* USBINTR bit fields */
#define INTR_IE  (1 << 0) /* interrupt enable */
#define INTR_HSE (1 << 2) /* host system error enable */
#define INTR_EIE (1 << 3) /* event interrupt enable */
#define INTR_PCE (1 << 4) /* port change enable */
#define INTR_TIE (1 << 8) /* transfer interrupt enable (MFINDEX wrap) */

/* CRCR (Command Ring Control) bit fields */
#define CRCR_RCS       (1 << 0) /* ring cycle state */
#define CRCR_CS        (1 << 1) /* command stop */
#define CRCR_CA        (1 << 2) /* command abort */
#define CRCR_CRR       (1 << 3) /* command ring running */
#define CRCR_ADDR_MASK 0xFFFFFFFFFFFFFFF0ULL

/* CONFIG bit fields */
#define CONFIG_MAX_SLOTS(x) ((x) & 0xff)
#define CONFIG_U3E          (1 << 8) /* U3 entry enable */
#define CONFIG_MIE          (1 << 9) /* maximum inter-packet interval enable */

/* PORTSC bit fields */
#define PORTSC_CCS         (1 << 0)   /* current connect status */
#define PORTSC_PED         (1 << 1)   /* port enable/disable */
#define PORTSC_OCA         (1 << 3)   /* over-current active */
#define PORTSC_PR          (1 << 4)   /* port reset */
#define PORTSC_PLS_MASK    (0xf << 5) /* port link state */
#define PORTSC_PLS(p)      (((p) >> 5) & 0xf)
#define PORTSC_PLS_SET(s)  ((s) << 5)
#define PORTSC_PP          (1 << 9) /* port power */
#define PORTSC_SPEED_MASK  (0xf << 10)
#define PORTSC_SPEED(p)    (((p) >> 10) & 0xf)
#define PORTSC_SPEED_FULL  (1 << 10)
#define PORTSC_SPEED_LOW   (2 << 10)
#define PORTSC_SPEED_HIGH  (3 << 10)
#define PORTSC_SPEED_SUPER (4 << 10)
#define PORTSC_PIC_MASK    (3 << 14) /* port indicator control */
#define PORTSC_LWS         (1 << 16) /* port link state write strobe */
#define PORTSC_CSC         (1 << 17) /* connect status change */
#define PORTSC_PEC         (1 << 18) /* port enable/disable change */
#define PORTSC_WRC         (1 << 19) /* warm port reset change */
#define PORTSC_OCC         (1 << 20) /* over-current change */
#define PORTSC_PRC         (1 << 21) /* port reset change */
#define PORTSC_PLC         (1 << 22) /* port link state change */
#define PORTSC_CEC         (1 << 23) /* port config error change */
#define PORTSC_CAS         (1 << 24) /* cold attach status */
#define PORTSC_WCE         (1 << 25) /* wake on connect enable */
#define PORTSC_WDE         (1 << 26) /* wake on disconnect enable */
#define PORTSC_WOE         (1 << 27) /* wake on over-current enable */
#define PORTSC_DR          (1 << 30) /* device removable */
#define PORTSC_WPR         (1 << 31) /* warm port reset */

#define PORTSC_CHANGE_MASK                                            \
    (PORTSC_CSC | PORTSC_PEC | PORTSC_WRC | PORTSC_OCC | PORTSC_PRC | \
     PORTSC_PLC | PORTSC_CEC)

/* Port link states (PLS) */
#define PLS_U0        0
#define PLS_U1        1
#define PLS_U2        2
#define PLS_U3        3
#define PLS_DISABLED  4
#define PLS_RXDETECT  5
#define PLS_INACTIVE  6
#define PLS_POLLING   7
#define PLS_RECOVERY  8
#define PLS_HOT_RESET 9
#define PLS_COMP_MODE 10
#define PLS_TEST_MODE 11
#define PLS_RESUME    15

/* =========================================================================
 * Runtime Registers (at MMIO base + RTSOFF)
 * ========================================================================= */
struct xhci_run_regs {
    u32 mfindex; /* microframe index */
    u32 reserved[7];
    struct xhci_intr_regs {
        u32 iman;   /* interrupter management */
        u32 imod;   /* interrupter moderation */
        u32 erstsz; /* event ring segment table size */
        u32 reserved;
        u64 erstba; /* event ring segment table base address */
        u64 erdp;   /* event ring dequeue pointer */
    } irs[0];       /* interrupter register sets */
};

/* IMAN bit fields */
#define IMAN_IP (1 << 0) /* interrupt pending */
#define IMAN_IE (1 << 1) /* interrupt enable */

/* IMOD bit fields */
#define IMOD_IVAL_MASK 0x0000ffff /* interrupt moderation interval */
#define IMOD_ICNT_MASK 0xffff0000 /* interrupt moderation counter */

/* =========================================================================
 * Doorbell Registers (at MMIO base + DBOFF)
 * =========================================================================
 * Each doorbell is a 32-bit register. Slot 0 is the command ring doorbell.
 * The low byte is the doorbell target, the high byte is the stream ID.
 */
#define DB_TARGET_HOST_COMMAND 0x0

/* =========================================================================
 * Transfer Request Block (TRB) - 16 bytes each
 * =========================================================================
 * TRBs are the fundamental unit of communication between software and the
 * xHC. They are arranged in rings (circular buffers).
 */
struct xhci_trb {
    u64 parameter;
    u32 status;
    u32 control;
} __attribute__((packed));

/* TRB type field (bits 10-15 of control) */
#define TRB_TYPE_MASK (0x3f << 10)
#define TRB_TYPE(p)   ((p) << 10) /* compose type into control word */
#define TRB_TYPE_GET(p) \
    (((p) >> 10) & 0x3f)        /* extract type from control word */
#define TRB_SLOT(p) ((p) << 24) /* slot ID in bits [31:24] */
#define TRB_EP(p)   ((p) << 16) /* endpoint ID in bits [20:16] */

#define TRB_CYCLE (1 << 0) /* cycle state bit */
#define TRB_TC    (1 << 1) /* toggle cycle (Link TRB) */
#define TRB_ISP   (1 << 2) /* interrupt on short packet */
#define TRB_CHAIN (1 << 4) /* chain bit */
#define TRB_IOC   (1 << 5) /* interrupt on completion */
#define TRB_IDT   (1 << 6) /* immediate data */
#define TRB_ENT   (1 << 9) /* evaluate next TRB */
#define TRB_BEI   (1 << 9) /* block event interrupt */

/* Configure Endpoint command flags */
#define TRB_DC (1 << 9) /* deconfigure all endpoints */

/* Address Device command flags */
#define TRB_BSA (1 << 9) /* block set address (configure ep0 only) */

/* TRB types */
enum xhci_trb_type {
    TRB_NORMAL = 1,
    TRB_SETUP_STAGE,
    TRB_DATA_STAGE,
    TRB_STATUS_STAGE,
    TRB_ISOCH,
    TRB_LINK,
    TRB_EVENT_DATA,
    TRB_NOOP,
    /* Command TRBs (type 9-23) */
    TRB_ENABLE_SLOT = 9,
    TRB_DISABLE_SLOT,
    TRB_ADDRESS_DEV,
    TRB_CONFIGURE_ENDPOINT,
    TRB_EVALUATE_CONTEXT,
    TRB_RESET_ENDPOINT,
    TRB_STOP_ENDPOINT,
    TRB_SET_TR_DEQUEUE,
    TRB_RESET_DEVICE,
    TRB_FORCE_HEADER,
    TRB_FORCE_STREAM,
    TRB_SET_LATENCY_TOLERANCE,
    TRB_VIEW_PORT,
    TRB_GET_PORT_BANDWIDTH,
    TRB_FORCE_EVENT,
    TRB_SET_MAX_EXIT_LATENCY = 22,
    TRB_NEGOTIATE_BANDWIDTH,
    /* Event TRBs (type 32-39) */
    TRB_TRANSFER_EVENT = 32,
    TRB_COMMAND_COMPLETION_EVENT,
    TRB_PORT_STATUS_CHANGE_EVENT,
    TRB_BANDWIDTH_REQUEST_EVENT,
    TRB_DOORBELL_EVENT,
    TRB_HOST_CONTROLLER_EVENT,
    TRB_DEVICE_NOTIFICATION_EVENT,
    TRB_MFINDEX_WRAP_EVENT,
};

/* Transfer TRB specific fields */
/* SETUP_STAGE TRB */
#define TRB_SETUP_BM_REQTYPE (1 << 16)
#define TRB_SETUP_DIR_IN     (1 << 16) /* data direction for control */

/* DATA_STAGE TRB */
#define TRB_DATA_DIR_IN (1 << 16)

/* Transfer Event TRB completion codes */
enum xhci_completion_code {
    COMP_SUCCESS = 1,
    COMP_PING_RESPONSE,
    COMP_BABBLE_DETECTED,
    COMP_TRANSACTION_ERROR,
    COMP_TRB_ERROR,
    COMP_STALL_ERROR,
    COMP_RESOURCE_ERROR,
    COMP_BANDWIDTH_ERROR,
    COMP_NO_SLOTS_ERROR,
    COMP_INVALID_STREAM_TYPE_ERROR,
    COMP_SLOT_NOT_ENABLED_ERROR,
    COMP_ENDPOINT_NOT_ENABLED_ERROR,
    COMP_SHORT_PACKET,
    COMP_RING_UNDERRUN,
    COMP_RING_OVERRUN,
    COMP_VF_EVENT_RING_FULL_ERROR,
    COMP_PARAMETER_ERROR,
    COMP_BANDWIDTH_OVERRUN_ERROR,
    COMP_CONTEXT_STATE_ERROR,
    COMP_NO_PING_RESPONSE_ERROR,
    COMP_EVENT_RING_FULL_ERROR,
    COMP_INCOMPATIBLE_DEVICE_ERROR,
    COMP_MISSED_SERVICE_ERROR,
    COMP_COMMAND_RING_STOPPED,
    COMP_COMMAND_ABORTED,
    COMP_STOPPED,
    COMP_STOPPED_LENGTH_INVALID,
    COMP_EXIT_LATENCY_TOO_LARGE_ERROR,
    COMP_ISOCH_BUFFER_OVERRUN = 31,
    COMP_EVENT_LOST_ERROR = 32,
    COMP_UNDEFINED_ERROR,
    COMP_INVALID_STREAM_ID_ERROR,
    COMP_SECONDARY_BANDWIDTH_ERROR,
    COMP_SPLIT_TRANSACTION_ERROR,
};

/* Command Completion Event specific */
/* Transfer Event specific - completion code is in bits 24-31 of status */
#define GET_COMP_CODE(p) (((p) >> 24) & 0xff)

/* =========================================================================
 * Ring Segment and Ring structures
 * ========================================================================= */
#define TRBS_PER_SEGMENT  256 /* default TRBs per ring segment */
#define TRB_SEGMENT_SIZE  (TRBS_PER_SEGMENT * sizeof(struct xhci_trb))
#define TRB_SEGMENT_ALIGN 64 /* xHC requires 64-byte alignment */

struct xhci_segment {
    struct xhci_trb* trbs;     /* virtual address of TRB array */
    phys_bytes dma;            /* physical address of TRB array */
    struct xhci_segment* next; /* next segment in ring */
    struct xhci_segment* prev; /* previous segment in ring */
};

struct xhci_ring {
    struct xhci_segment* first_seg;
    struct xhci_segment* enqueue_seg;
    struct xhci_trb* enqueue; /* next TRB to fill */
    struct xhci_segment* dequeue_seg;
    struct xhci_trb* dequeue; /* next TRB to process */
    unsigned int num_segs;
    unsigned int cycle_state; /* current cycle bit */
    unsigned int stream_id;   /* stream ID (0 for non-stream) */
};

/* =========================================================================
 * Event Ring Segment Table (ERST)
 * ========================================================================= */
struct xhci_erst_entry {
    u64 addr; /* segment address */
    u32 size; /* segment size (in TRBs) */
    u32 reserved;
} __attribute__((packed));

struct xhci_erst {
    struct xhci_erst_entry* entries;
    phys_bytes dma;
    unsigned int num_entries;
    struct xhci_ring* ring;
};

/* =========================================================================
 * Context Structures
 * =========================================================================
 * xHCI uses context structures to describe device slot and endpoint state.
 * Contexts are 32 or 64 bytes each depending on HCCPARAMS1.
 */
#define XHCI_MAX_SLOTS     256
#define XHCI_MAX_ENDPOINTS 32 /* 16 IN + 16 OUT */
#define XHCI_CTX_SIZE_32   32
#define XHCI_CTX_SIZE_64   64

/* Slot Context (first context in device context) */
struct xhci_slot_ctx {
    u32 info1;       /* slot info 1 */
    u32 info2;       /* slot info 2 */
    u32 tt_info;     /* transaction translator info */
    u32 state;       /* slot state */
    u32 reserved[4]; /* xHC private; up to 4 more dwords */
};

#define SLOT_INFO_DEV_SPEED(s)    ((s) & 0xf)
#define SLOT_INFO_MAX_EXIT_LAT(p) (((p) >> 4) & 0xffff)
#define SLOT_INFO_RO(p)           (((p) >> 24) & 0x1)
#define SLOT_INFO_NUM_PORTS(p)    (((p) >> 24) & 0xff)
#define SLOT_INFO_ROUTE_STR(p)    (((p) >> 25) & 0x1)

#define SLOT_STATE_DISABLED   0
#define SLOT_STATE_ENABLED    1
#define SLOT_STATE_DEFAULT    2
#define SLOT_STATE_ADDRESSED  3
#define SLOT_STATE_CONFIGURED 4

/* Endpoint Context */
struct xhci_ep_ctx {
    u32 info1;       /* endpoint info 1 */
    u32 info2;       /* endpoint info 2 */
    u64 deq;         /* transfer ring dequeue pointer */
    u32 tx_info;     /* average TRB length / max ESIT payload */
    u32 reserved[3]; /* xHC private */
};

#define EP_TYPE_MASK     (0x7 << 3)
#define EP_TYPE(p)       (((p) >> 3) & 0x7)
#define EP_TYPE_ISOC_OUT 1
#define EP_TYPE_BULK_OUT 2
#define EP_TYPE_INT_OUT  3
#define EP_TYPE_CONTROL  4
#define EP_TYPE_ISOC_IN  5
#define EP_TYPE_BULK_IN  6
#define EP_TYPE_INT_IN   7

#define EP_MULT(p)         ((p) & 0x3)
#define EP_MAX_PSTREAMS(p) (((p) >> 10) & 0x1f)
#define EP_LSA             (1 << 15)

#define EP_STATE_DISABLED 0
#define EP_STATE_RUNNING  1
#define EP_STATE_HALTED   2
#define EP_STATE_STOPPED  3
#define EP_STATE_ERROR    4

/* Input Control Context (first context in input context) */
struct xhci_input_ctrl_ctx {
    u32 drop_flags; /* bit n set = drop context n */
    u32 add_flags;  /* bit n set = add context n */
    u32 reserved[6];
};

/* Device Context: slot context + up to 31 endpoint contexts */
struct xhci_device_ctx {
    struct xhci_slot_ctx slot;
    struct xhci_ep_ctx ep[XHCI_MAX_ENDPOINTS];
};

/* Input Context: input control context + device context */
struct xhci_input_ctx {
    struct xhci_input_ctrl_ctx ctrl;
    struct xhci_device_ctx dev;
};

/* =========================================================================
 * Virtual Device / Endpoint tracking (software side)
 * ========================================================================= */
struct xhci_virt_ep {
    struct xhci_ring* ring;       /* transfer ring for this endpoint */
    struct xhci_ep_ctx* ctx;      /* pointer into device context */
    unsigned int ep_state;        /* EP_STATE_* */
    struct usb_host_endpoint* ep; /* back-pointer for URB lookup */
    int skip;                     /* skip processing (error recovery) */
    int recovery_state;           /* nonzero blocks publication/doorbells */
    phys_bytes recovery_cmd;
    struct urb* recovery_urb;     /* retains DMA mappings until Set Dequeue */
    int recovery_status;
};

struct xhci_virt_device {
    struct usb_device* udev;         /* corresponding USB device */
    struct xhci_device_ctx* out_ctx; /* output device context (DMA) */
    phys_bytes out_ctx_dma;
    struct xhci_input_ctx* in_ctx; /* input context (DMA) */
    phys_bytes in_ctx_dma;
    struct xhci_virt_ep eps[XHCI_MAX_ENDPOINTS];
    unsigned int slot_id;
    int enabled;
};

/* =========================================================================
 * xHCI HCD private data (stored in usb_hcd->hcd_priv)
 * ========================================================================= */
struct xhci_hcd {
    struct xhci_cap_regs* cap_regs; /* capability registers */
    struct xhci_op_regs* op_regs;   /* operational registers */
    struct xhci_run_regs* run_regs; /* runtime registers */
    void* db_base;                  /* doorbell register base */

    unsigned int num_ports;        /* number of root hub ports */
    unsigned int num_slots;        /* max number of device slots */
    unsigned int num_intrs;        /* number of interrupters */
    unsigned int max_scratch_bufs; /* scratch buffer count */
    unsigned int page_size;        /* xHC page size */
    int context_size;              /* 32 or 64 bytes per context */

    /* Device Context Base Address Array (DCBAA) */
    u64* dcbaa;           /* virtual address */
    phys_bytes dcbaa_dma; /* physical address */

    /* Command ring */
    struct xhci_ring* cmd_ring;
    u32 cmd_ring_reserved; /* reserved TRBs in flight */

    /* DMA pools */
    struct dma_pool* segment_pool;
    struct dma_pool* in_ctx_pool;
    struct dma_pool* out_ctx_pool;

    /* Event ring (interrupter 0) */
    struct xhci_erst erst;
    struct xhci_ring* event_ring;

    /* Scratchpad buffers */
    u64* scratchpad; /* array of scratchpad buffer addrs */
    phys_bytes scratchpad_dma;
    void** scratchpad_bufs;

    /* Per-slot device tracking */
    struct xhci_virt_device* devs[XHCI_MAX_SLOTS];

    /* Port array */
    struct xhci_port {
        unsigned int hw_portnum; /* hardware port number (0-based) */
        int majrev;              /* USB major revision */
        int minrev;              /* USB minor revision */
    }* ports;

    /* Halt/restart state */
    int hcc_params; /* cached HCCPARAMS1 */
    u32 usbcmd;     /* cached USBCMD */

    /* Command completion wait mechanism */
    async_worker_id_t cmd_wid; /* worker waiting for completion */
    volatile int cmd_done;     /* completion flag */
    u32 cmd_status;            /* completion code */
    u32 cmd_slot_id;           /* slot ID from Enable Slot */
    u64 cmd_param;             /* parameter from completion event */
};

static inline struct xhci_hcd* hcd_to_xhci(struct usb_hcd* hcd)
{
    return (struct xhci_hcd*)(hcd->hcd_priv);
}

static inline struct usb_hcd* xhci_to_hcd(const struct xhci_hcd* xhci)
{
    return list_entry((void*)xhci, struct usb_hcd, hcd_priv);
}

/* Register access helpers */
static inline u32 xhci_readl(const struct xhci_hcd* xhci,
                             const volatile u32* reg)
{
    return readl((const void*)reg);
}

static inline void xhci_writel(const struct xhci_hcd* xhci, volatile u32* reg,
                               u32 val)
{
    writel((void*)reg, val);
}

static inline u64 xhci_readq(const struct xhci_hcd* xhci,
                             const volatile u64* reg)
{
    return readq((const void*)reg);
}

static inline void xhci_writeq(const struct xhci_hcd* xhci, volatile u64* reg,
                               u64 val)
{
    writeq((void*)reg, val);
}

static inline volatile u32* xhci_portsc_addr(struct xhci_hcd* xhci, int port)
{
    /* Each port's register occupies 0x10 bytes; PORTSC is the first u32 */
    return (volatile u32*)((char*)&xhci->op_regs->portsc_base[port * 4]);
}

static inline volatile u32* xhci_db_addr(struct xhci_hcd* xhci, int slot_id)
{
    return (volatile u32*)xhci->db_base + slot_id;
}

/* =========================================================================
 * Function declarations
 * ========================================================================= */

/* xhci-hcd.c */
void xhci_init_driver(struct hc_driver* driver);
int xhci_comp_to_errno(u32 comp_code);

/* xhci-mem.c */
int xhci_mem_init(struct xhci_hcd* xhci);
void xhci_mem_cleanup(struct xhci_hcd* xhci);
struct xhci_ring* xhci_ring_alloc(struct xhci_hcd* xhci, unsigned int num_segs,
                                  int is_event);
void xhci_ring_free(struct xhci_hcd* xhci, struct xhci_ring* ring);
int xhci_alloc_dev(struct xhci_hcd* xhci, struct usb_device* udev,
                   unsigned int slot_id);
void xhci_free_dev(struct xhci_hcd* xhci, struct usb_device* udev);
size_t xhci_input_ctx_size(struct xhci_hcd* xhci);

/* xhci-ring.c */
int xhci_ring_enqueue(struct xhci_ring* ring, struct xhci_trb* trb);
void xhci_ring_cmd_db(struct xhci_hcd* xhci);
void xhci_ring_ep_db(struct xhci_hcd* xhci, unsigned int slot_id,
                     unsigned int ep_index);
int xhci_queue_urb(struct xhci_hcd* xhci, struct urb* urb, unsigned int slot_id,
                   unsigned int ep_index);
int xhci_handle_event(struct xhci_hcd* xhci, struct xhci_trb* event);
int xhci_wait_cmd_completion(struct xhci_hcd* xhci);
int xhci_queue_command(struct xhci_hcd* xhci, u32 field1, u32 field2,
                       u32 field3, u32 field4);
int xhci_cmd_enable_slot(struct xhci_hcd* xhci, unsigned int* slot_id);
int xhci_cmd_address_dev(struct xhci_hcd* xhci, phys_bytes in_ctx_dma,
                         unsigned int slot_id);
int xhci_cmd_configure_ep(struct xhci_hcd* xhci, phys_bytes in_ctx_dma,
                          unsigned int slot_id, int config_change);
int xhci_cmd_reset_ep(struct xhci_hcd* xhci, unsigned int slot_id,
                      unsigned int ep_index);

/* xhci-hub.c */
int xhci_hub_status_data(struct usb_hcd* hcd, char* buf);
int xhci_hub_control(struct usb_hcd* hcd, u16 typeReq, u16 wValue, u16 wIndex,
                     char* buf, u16 wLength);
void xhci_handle_port_change(struct xhci_hcd* xhci);

#if CONFIG_USB_XHCI_HCD_PCI
int xhci_pci_init(void);
int xhci_pci_probe(int devind);
#endif

#endif /* _USBD_XHCI_H_ */
