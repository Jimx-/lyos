#ifndef _NVME_NVME_H_
#define _NVME_NVME_H_

enum {
    NVME_REG_CAP = 0x0000,     /* Controller Capabilities */
    NVME_REG_VS = 0x0008,      /* Version */
    NVME_REG_INTMS = 0x000c,   /* Interrupt Mask Set */
    NVME_REG_INTMC = 0x0010,   /* Interrupt Mask Clear */
    NVME_REG_CC = 0x0014,      /* Controller Configuration */
    NVME_REG_CSTS = 0x001c,    /* Controller Status */
    NVME_REG_NSSR = 0x0020,    /* NVM Subsystem Reset */
    NVME_REG_AQA = 0x0024,     /* Admin Queue Attributes */
    NVME_REG_ASQ = 0x0028,     /* Admin SQ Base Address */
    NVME_REG_ACQ = 0x0030,     /* Admin CQ Base Address */
    NVME_REG_CMBLOC = 0x0038,  /* Controller Memory Buffer Location */
    NVME_REG_CMBSZ = 0x003c,   /* Controller Memory Buffer Size */
    NVME_REG_BPINFO = 0x0040,  /* Boot Partition Information */
    NVME_REG_BPRSEL = 0x0044,  /* Boot Partition Read Select */
    NVME_REG_BPMBL = 0x0048,   /* Boot Partition Memory Buffer
                                * Location
                                */
    NVME_REG_PMRCAP = 0x0e00,  /* Persistent Memory Capabilities */
    NVME_REG_PMRCTL = 0x0e04,  /* Persistent Memory Region Control */
    NVME_REG_PMRSTS = 0x0e08,  /* Persistent Memory Region Status */
    NVME_REG_PMREBS = 0x0e0c,  /* Persistent Memory Region Elasticity
                                * Buffer Size
                                */
    NVME_REG_PMRSWTP = 0x0e10, /* Persistent Memory Region Sustained
                                * Write Throughput
                                */
    NVME_REG_DBS = 0x1000,     /* SQ 0 Tail Doorbell */
};

#define NVME_CAP_MQES(cap)    ((cap)&0xffff)
#define NVME_CAP_TIMEOUT(cap) (((cap) >> 24) & 0xff)
#define NVME_CAP_STRIDE(cap)  (((cap) >> 32) & 0xf)
#define NVME_CAP_NSSRC(cap)   (((cap) >> 36) & 0x1)
#define NVME_CAP_MPSMIN(cap)  (((cap) >> 48) & 0xf)
#define NVME_CAP_MPSMAX(cap)  (((cap) >> 52) & 0xf)

/*
 * Submission and Completion Queue Entry Sizes for the NVM command set.
 * (In bytes and specified as a power of two (2^n)).
 */
#define NVME_ADM_SQES   6
#define NVME_NVM_IOSQES 6
#define NVME_NVM_IOCQES 4

#define NVME_AQ_DEPTH 32

enum {
    NVME_CC_ENABLE = 1 << 0,
    NVME_CC_CSS_NVM = 0 << 4,
    NVME_CC_EN_SHIFT = 0,
    NVME_CC_CSS_SHIFT = 4,
    NVME_CC_MPS_SHIFT = 7,
    NVME_CC_AMS_SHIFT = 11,
    NVME_CC_SHN_SHIFT = 14,
    NVME_CC_IOSQES_SHIFT = 16,
    NVME_CC_IOCQES_SHIFT = 20,
    NVME_CC_AMS_RR = 0 << NVME_CC_AMS_SHIFT,
    NVME_CC_AMS_WRRU = 1 << NVME_CC_AMS_SHIFT,
    NVME_CC_AMS_VS = 7 << NVME_CC_AMS_SHIFT,
    NVME_CC_SHN_NONE = 0 << NVME_CC_SHN_SHIFT,
    NVME_CC_SHN_NORMAL = 1 << NVME_CC_SHN_SHIFT,
    NVME_CC_SHN_ABRUPT = 2 << NVME_CC_SHN_SHIFT,
    NVME_CC_SHN_MASK = 3 << NVME_CC_SHN_SHIFT,
    NVME_CC_IOSQES = NVME_NVM_IOSQES << NVME_CC_IOSQES_SHIFT,
    NVME_CC_IOCQES = NVME_NVM_IOCQES << NVME_CC_IOCQES_SHIFT,
    NVME_CSTS_RDY = 1 << 0,
    NVME_CSTS_CFS = 1 << 1,
    NVME_CSTS_NSSRO = 1 << 4,
    NVME_CSTS_PP = 1 << 5,
    NVME_CSTS_SHST_NORMAL = 0 << 2,
    NVME_CSTS_SHST_OCCUR = 1 << 2,
    NVME_CSTS_SHST_CMPLT = 2 << 2,
    NVME_CSTS_SHST_MASK = 3 << 2,
};

struct nvme_id_power_state {
    __le16 max_power; /* centiwatts */
    __u8 rsvd2;
    __u8 flags;
    __le32 entry_lat; /* microseconds */
    __le32 exit_lat;  /* microseconds */
    __u8 read_tput;
    __u8 read_lat;
    __u8 write_tput;
    __u8 write_lat;
    __le16 idle_power;
    __u8 idle_scale;
    __u8 rsvd19;
    __le16 active_power;
    __u8 active_work_scale;
    __u8 rsvd23[9];
};

struct nvme_id_ctrl {
    __le16 vid;
    __le16 ssvid;
    char sn[20];
    char mn[40];
    char fr[8];
    __u8 rab;
    __u8 ieee[3];
    __u8 cmic;
    __u8 mdts;
    __le16 cntlid;
    __le32 ver;
    __le32 rtd3r;
    __le32 rtd3e;
    __le32 oaes;
    __le32 ctratt;
    __u8 rsvd100[28];
    __le16 crdt1;
    __le16 crdt2;
    __le16 crdt3;
    __u8 rsvd134[122];
    __le16 oacs;
    __u8 acl;
    __u8 aerl;
    __u8 frmw;
    __u8 lpa;
    __u8 elpe;
    __u8 npss;
    __u8 avscc;
    __u8 apsta;
    __le16 wctemp;
    __le16 cctemp;
    __le16 mtfa;
    __le32 hmpre;
    __le32 hmmin;
    __u8 tnvmcap[16];
    __u8 unvmcap[16];
    __le32 rpmbs;
    __le16 edstt;
    __u8 dsto;
    __u8 fwug;
    __le16 kas;
    __le16 hctma;
    __le16 mntmt;
    __le16 mxtmt;
    __le32 sanicap;
    __le32 hmminds;
    __le16 hmmaxd;
    __u8 rsvd338[4];
    __u8 anatt;
    __u8 anacap;
    __le32 anagrpmax;
    __le32 nanagrpid;
    __u8 rsvd352[160];
    __u8 sqes;
    __u8 cqes;
    __le16 maxcmd;
    __le32 nn;
    __le16 oncs;
    __le16 fuses;
    __u8 fna;
    __u8 vwc;
    __le16 awun;
    __le16 awupf;
    __u8 nvscc;
    __u8 nwpc;
    __le16 acwu;
    __u8 rsvd534[2];
    __le32 sgls;
    __le32 mnan;
    __u8 rsvd544[224];
    char subnqn[256];
    __u8 rsvd1024[768];
    __le32 ioccsz;
    __le32 iorcsz;
    __le16 icdoff;
    __u8 ctrattr;
    __u8 msdbd;
    __u8 rsvd1804[244];
    struct nvme_id_power_state psd[32];
    __u8 vs[1024];
};

struct nvme_lbaf {
    __le16 ms;
    __u8 ds;
    __u8 rp;
};

struct nvme_id_ns {
    __le64 nsze;
    __le64 ncap;
    __le64 nuse;
    __u8 nsfeat;
    __u8 nlbaf;
    __u8 flbas;
    __u8 mc;
    __u8 dpc;
    __u8 dps;
    __u8 nmic;
    __u8 rescap;
    __u8 fpi;
    __u8 dlfeat;
    __le16 nawun;
    __le16 nawupf;
    __le16 nacwu;
    __le16 nabsn;
    __le16 nabo;
    __le16 nabspf;
    __le16 noiob;
    __u8 nvmcap[16];
    __le16 npwg;
    __le16 npwa;
    __le16 npdg;
    __le16 npda;
    __le16 nows;
    __u8 rsvd74[18];
    __le32 anagrpid;
    __u8 rsvd96[3];
    __u8 nsattr;
    __le16 nvmsetid;
    __le16 endgid;
    __u8 nguid[16];
    __u8 eui64[8];
    struct nvme_lbaf lbaf[16];
    __u8 rsvd192[192];
    __u8 vs[3712];
};

enum {
    NVME_ID_CNS_NS = 0x00,
    NVME_ID_CNS_CTRL = 0x01,
    NVME_ID_CNS_NS_ACTIVE_LIST = 0x02,
    NVME_ID_CNS_NS_DESC_LIST = 0x03,
    NVME_ID_CNS_NS_PRESENT_LIST = 0x10,
    NVME_ID_CNS_NS_PRESENT = 0x11,
    NVME_ID_CNS_CTRL_NS_LIST = 0x12,
    NVME_ID_CNS_CTRL_LIST = 0x13,
    NVME_ID_CNS_SCNDRY_CTRL_LIST = 0x15,
    NVME_ID_CNS_NS_GRANULARITY = 0x16,
    NVME_ID_CNS_UUID_LIST = 0x17,
};

enum {
    NVME_NS_FEAT_THIN = 1 << 0,
    NVME_NS_FLBAS_LBA_MASK = 0xf,
    NVME_NS_FLBAS_META_EXT = 0x10,
    NVME_LBAF_RP_BEST = 0,
    NVME_LBAF_RP_BETTER = 1,
    NVME_LBAF_RP_GOOD = 2,
    NVME_LBAF_RP_DEGRADED = 3,
    NVME_NS_DPC_PI_LAST = 1 << 4,
    NVME_NS_DPC_PI_FIRST = 1 << 3,
    NVME_NS_DPC_PI_TYPE3 = 1 << 2,
    NVME_NS_DPC_PI_TYPE2 = 1 << 1,
    NVME_NS_DPC_PI_TYPE1 = 1 << 0,
    NVME_NS_DPS_PI_FIRST = 1 << 3,
    NVME_NS_DPS_PI_MASK = 0x7,
    NVME_NS_DPS_PI_TYPE1 = 1,
    NVME_NS_DPS_PI_TYPE2 = 2,
    NVME_NS_DPS_PI_TYPE3 = 3,
};

union nvme_data_ptr {
    struct {
        __le64 prp1;
        __le64 prp2;
    };
};

/* I/O commands */

enum nvme_opcode {
    nvme_cmd_flush = 0x00,
    nvme_cmd_write = 0x01,
    nvme_cmd_read = 0x02,
    nvme_cmd_write_uncor = 0x04,
    nvme_cmd_compare = 0x05,
    nvme_cmd_write_zeroes = 0x08,
    nvme_cmd_dsm = 0x09,
    nvme_cmd_verify = 0x0c,
    nvme_cmd_resv_register = 0x0d,
    nvme_cmd_resv_report = 0x0e,
    nvme_cmd_resv_acquire = 0x11,
    nvme_cmd_resv_release = 0x15,
};

struct nvme_common_command {
    __u8 opcode;
    __u8 flags;
    __u16 command_id;
    __le32 nsid;
    __le32 cdw2[2];
    __le64 metadata;
    union nvme_data_ptr dptr;
    __le32 cdw10;
    __le32 cdw11;
    __le32 cdw12;
    __le32 cdw13;
    __le32 cdw14;
    __le32 cdw15;
};

struct nvme_rw_command {
    __u8 opcode;
    __u8 flags;
    __u16 command_id;
    __le32 nsid;
    __u64 rsvd2;
    __le64 metadata;
    union nvme_data_ptr dptr;
    __le64 slba;
    __le16 length;
    __le16 control;
    __le32 dsmgmt;
    __le32 reftag;
    __le16 apptag;
    __le16 appmask;
};

/* Admin commands */

enum nvme_admin_opcode {
    nvme_admin_delete_sq = 0x00,
    nvme_admin_create_sq = 0x01,
    nvme_admin_get_log_page = 0x02,
    nvme_admin_delete_cq = 0x04,
    nvme_admin_create_cq = 0x05,
    nvme_admin_identify = 0x06,
    nvme_admin_abort_cmd = 0x08,
    nvme_admin_set_features = 0x09,
    nvme_admin_get_features = 0x0a,
    nvme_admin_async_event = 0x0c,
    nvme_admin_ns_mgmt = 0x0d,
    nvme_admin_activate_fw = 0x10,
    nvme_admin_download_fw = 0x11,
    nvme_admin_dev_self_test = 0x14,
    nvme_admin_ns_attach = 0x15,
    nvme_admin_keep_alive = 0x18,
    nvme_admin_directive_send = 0x19,
    nvme_admin_directive_recv = 0x1a,
    nvme_admin_virtual_mgmt = 0x1c,
    nvme_admin_nvme_mi_send = 0x1d,
    nvme_admin_nvme_mi_recv = 0x1e,
    nvme_admin_dbbuf = 0x7C,
    nvme_admin_format_nvm = 0x80,
    nvme_admin_security_send = 0x81,
    nvme_admin_security_recv = 0x82,
    nvme_admin_sanitize_nvm = 0x84,
    nvme_admin_get_lba_status = 0x86,
};

enum {
    NVME_QUEUE_PHYS_CONTIG = (1 << 0),
    NVME_CQ_IRQ_ENABLED = (1 << 1),
    NVME_SQ_PRIO_URGENT = (0 << 1),
    NVME_SQ_PRIO_HIGH = (1 << 1),
    NVME_SQ_PRIO_MEDIUM = (2 << 1),
    NVME_SQ_PRIO_LOW = (3 << 1),
    NVME_FEAT_ARBITRATION = 0x01,
    NVME_FEAT_POWER_MGMT = 0x02,
    NVME_FEAT_LBA_RANGE = 0x03,
    NVME_FEAT_TEMP_THRESH = 0x04,
    NVME_FEAT_ERR_RECOVERY = 0x05,
    NVME_FEAT_VOLATILE_WC = 0x06,
    NVME_FEAT_NUM_QUEUES = 0x07,
    NVME_FEAT_IRQ_COALESCE = 0x08,
    NVME_FEAT_IRQ_CONFIG = 0x09,
    NVME_FEAT_WRITE_ATOMIC = 0x0a,
    NVME_FEAT_ASYNC_EVENT = 0x0b,
    NVME_FEAT_AUTO_PST = 0x0c,
    NVME_FEAT_HOST_MEM_BUF = 0x0d,
    NVME_FEAT_TIMESTAMP = 0x0e,
    NVME_FEAT_KATO = 0x0f,
    NVME_FEAT_HCTM = 0x10,
    NVME_FEAT_NOPSC = 0x11,
    NVME_FEAT_RRL = 0x12,
    NVME_FEAT_PLM_CONFIG = 0x13,
    NVME_FEAT_PLM_WINDOW = 0x14,
    NVME_FEAT_HOST_BEHAVIOR = 0x16,
    NVME_FEAT_SANITIZE = 0x17,
    NVME_FEAT_SW_PROGRESS = 0x80,
    NVME_FEAT_HOST_ID = 0x81,
    NVME_FEAT_RESV_MASK = 0x82,
    NVME_FEAT_RESV_PERSIST = 0x83,
    NVME_FEAT_WRITE_PROTECT = 0x84,
    NVME_LOG_ERROR = 0x01,
    NVME_LOG_SMART = 0x02,
    NVME_LOG_FW_SLOT = 0x03,
    NVME_LOG_CHANGED_NS = 0x04,
    NVME_LOG_CMD_EFFECTS = 0x05,
    NVME_LOG_DEVICE_SELF_TEST = 0x06,
    NVME_LOG_TELEMETRY_HOST = 0x07,
    NVME_LOG_TELEMETRY_CTRL = 0x08,
    NVME_LOG_ENDURANCE_GROUP = 0x09,
    NVME_LOG_ANA = 0x0c,
    NVME_LOG_DISC = 0x70,
    NVME_LOG_RESERVATION = 0x80,
    NVME_FWACT_REPL = (0 << 3),
    NVME_FWACT_REPL_ACTV = (1 << 3),
    NVME_FWACT_ACTV = (2 << 3),
};

struct nvme_identify {
    __u8 opcode;
    __u8 flags;
    __u16 command_id;
    __le32 nsid;
    __u64 rsvd2[2];
    union nvme_data_ptr dptr;
    __u8 cns;
    __u8 rsvd3;
    __le16 ctrlid;
    __u32 rsvd11[5];
};

#define NVME_IDENTIFY_DATA_SIZE 4096

struct nvme_features {
    __u8 opcode;
    __u8 flags;
    __u16 command_id;
    __le32 nsid;
    __u64 rsvd2[2];
    union nvme_data_ptr dptr;
    __le32 fid;
    __le32 dword11;
    __le32 dword12;
    __le32 dword13;
    __le32 dword14;
    __le32 dword15;
};

struct nvme_create_cq {
    __u8 opcode;
    __u8 flags;
    __u16 command_id;
    __u32 rsvd1[5];
    __le64 prp1;
    __u64 rsvd8;
    __le16 cqid;
    __le16 qsize;
    __le16 cq_flags;
    __le16 irq_vector;
    __u32 rsvd12[4];
};

struct nvme_create_sq {
    __u8 opcode;
    __u8 flags;
    __u16 command_id;
    __u32 rsvd1[5];
    __le64 prp1;
    __u64 rsvd8;
    __le16 sqid;
    __le16 qsize;
    __le16 sq_flags;
    __le16 cqid;
    __u32 rsvd12[4];
};

struct nvme_delete_queue {
    __u8 opcode;
    __u8 flags;
    __u16 command_id;
    __u32 rsvd1[9];
    __le16 qid;
    __u16 rsvd10;
    __u32 rsvd11[5];
};

struct nvme_command {
    union {
        struct nvme_common_command common;
        struct nvme_rw_command rw;
        struct nvme_identify identify;
        struct nvme_features features;
        struct nvme_create_cq create_cq;
        struct nvme_create_sq create_sq;
        struct nvme_delete_queue delete_queue;
    };
};

enum {
    /*
     * Generic Command Status:
     */
    NVME_SC_SUCCESS = 0x0,
    NVME_SC_INVALID_OPCODE = 0x1,
    NVME_SC_INVALID_FIELD = 0x2,
    NVME_SC_CMDID_CONFLICT = 0x3,
    NVME_SC_DATA_XFER_ERROR = 0x4,
    NVME_SC_POWER_LOSS = 0x5,
    NVME_SC_INTERNAL = 0x6,
    NVME_SC_ABORT_REQ = 0x7,
    NVME_SC_ABORT_QUEUE = 0x8,
    NVME_SC_FUSED_FAIL = 0x9,
    NVME_SC_FUSED_MISSING = 0xa,
    NVME_SC_INVALID_NS = 0xb,
    NVME_SC_CMD_SEQ_ERROR = 0xc,
    NVME_SC_SGL_INVALID_LAST = 0xd,
    NVME_SC_SGL_INVALID_COUNT = 0xe,
    NVME_SC_SGL_INVALID_DATA = 0xf,
    NVME_SC_SGL_INVALID_METADATA = 0x10,
    NVME_SC_SGL_INVALID_TYPE = 0x11,

    NVME_SC_SGL_INVALID_OFFSET = 0x16,
    NVME_SC_SGL_INVALID_SUBTYPE = 0x17,

    NVME_SC_SANITIZE_FAILED = 0x1C,
    NVME_SC_SANITIZE_IN_PROGRESS = 0x1D,

    NVME_SC_NS_WRITE_PROTECTED = 0x20,
    NVME_SC_CMD_INTERRUPTED = 0x21,

    NVME_SC_LBA_RANGE = 0x80,
    NVME_SC_CAP_EXCEEDED = 0x81,
    NVME_SC_NS_NOT_READY = 0x82,
    NVME_SC_RESERVATION_CONFLICT = 0x83,

    /*
     * Command Specific Status:
     */
    NVME_SC_CQ_INVALID = 0x100,
    NVME_SC_QID_INVALID = 0x101,
    NVME_SC_QUEUE_SIZE = 0x102,
    NVME_SC_ABORT_LIMIT = 0x103,
    NVME_SC_ABORT_MISSING = 0x104,
    NVME_SC_ASYNC_LIMIT = 0x105,
    NVME_SC_FIRMWARE_SLOT = 0x106,
    NVME_SC_FIRMWARE_IMAGE = 0x107,
    NVME_SC_INVALID_VECTOR = 0x108,
    NVME_SC_INVALID_LOG_PAGE = 0x109,
    NVME_SC_INVALID_FORMAT = 0x10a,
    NVME_SC_FW_NEEDS_CONV_RESET = 0x10b,
    NVME_SC_INVALID_QUEUE = 0x10c,
    NVME_SC_FEATURE_NOT_SAVEABLE = 0x10d,
    NVME_SC_FEATURE_NOT_CHANGEABLE = 0x10e,
    NVME_SC_FEATURE_NOT_PER_NS = 0x10f,
    NVME_SC_FW_NEEDS_SUBSYS_RESET = 0x110,
    NVME_SC_FW_NEEDS_RESET = 0x111,
    NVME_SC_FW_NEEDS_MAX_TIME = 0x112,
    NVME_SC_FW_ACTIVATE_PROHIBITED = 0x113,
    NVME_SC_OVERLAPPING_RANGE = 0x114,
    NVME_SC_NS_INSUFFICIENT_CAP = 0x115,
    NVME_SC_NS_ID_UNAVAILABLE = 0x116,
    NVME_SC_NS_ALREADY_ATTACHED = 0x118,
    NVME_SC_NS_IS_PRIVATE = 0x119,
    NVME_SC_NS_NOT_ATTACHED = 0x11a,
    NVME_SC_THIN_PROV_NOT_SUPP = 0x11b,
    NVME_SC_CTRL_LIST_INVALID = 0x11c,
    NVME_SC_BP_WRITE_PROHIBITED = 0x11e,
    NVME_SC_PMR_SAN_PROHIBITED = 0x123,

    /*
     * I/O Command Set Specific - NVM commands:
     */
    NVME_SC_BAD_ATTRIBUTES = 0x180,
    NVME_SC_INVALID_PI = 0x181,
    NVME_SC_READ_ONLY = 0x182,
    NVME_SC_ONCS_NOT_SUPPORTED = 0x183,

    /*
     * I/O Command Set Specific - Fabrics commands:
     */
    NVME_SC_CONNECT_FORMAT = 0x180,
    NVME_SC_CONNECT_CTRL_BUSY = 0x181,
    NVME_SC_CONNECT_INVALID_PARAM = 0x182,
    NVME_SC_CONNECT_RESTART_DISC = 0x183,
    NVME_SC_CONNECT_INVALID_HOST = 0x184,

    NVME_SC_DISCOVERY_RESTART = 0x190,
    NVME_SC_AUTH_REQUIRED = 0x191,

    /*
     * Media and Data Integrity Errors:
     */
    NVME_SC_WRITE_FAULT = 0x280,
    NVME_SC_READ_ERROR = 0x281,
    NVME_SC_GUARD_CHECK = 0x282,
    NVME_SC_APPTAG_CHECK = 0x283,
    NVME_SC_REFTAG_CHECK = 0x284,
    NVME_SC_COMPARE_FAILED = 0x285,
    NVME_SC_ACCESS_DENIED = 0x286,
    NVME_SC_UNWRITTEN_BLOCK = 0x287,

    /*
     * Path-related Errors:
     */
    NVME_SC_ANA_PERSISTENT_LOSS = 0x301,
    NVME_SC_ANA_INACCESSIBLE = 0x302,
    NVME_SC_ANA_TRANSITION = 0x303,
    NVME_SC_HOST_PATH_ERROR = 0x370,
    NVME_SC_HOST_ABORTED_CMD = 0x371,

    NVME_SC_CRD = 0x1800,
    NVME_SC_DNR = 0x4000,
};

struct nvme_completion {
    /*
     * Used by Admin and Fabrics commands to return data:
     */
    union nvme_result {
        __le16 u16;
        __le32 u32;
        __le64 u64;
    } result;
    __le16 sq_head;   /* how much of this queue may be reclaimed */
    __le16 sq_id;     /* submission queue that generated this entry */
    __u16 command_id; /* of the command which completed */
    __le16 status;    /* did the command fail, and if so, why? */
};

#endif
