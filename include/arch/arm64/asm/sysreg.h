#ifndef _ARCH_SYSREG_H_
#define _ARCH_SYSREG_H_

#define Op0_shift 19
#define Op0_mask  0x3
#define Op1_shift 16
#define Op1_mask  0x7
#define CRn_shift 12
#define CRn_mask  0xf
#define CRm_shift 8
#define CRm_mask  0xf
#define Op2_shift 5
#define Op2_mask  0x7

#define sys_reg(op0, op1, crn, crm, op2)                                  \
    (((op0) << Op0_shift) | ((op1) << Op1_shift) | ((crn) << CRn_shift) | \
     ((crm) << CRm_shift) | ((op2) << Op2_shift))

#define __emit_inst(x) ".long " #x "\n\t"

#define SCTLR_ELx_ATA (BIT(43))

#define SCTLR_ELx_ENIA_SHIFT 31

#define SCTLR_ELx_ITFSB (BIT(37))
#define SCTLR_ELx_ENIA  (BIT(SCTLR_ELx_ENIA_SHIFT))
#define SCTLR_ELx_ENIB  (BIT(30))
#define SCTLR_ELx_ENDA  (BIT(27))
#define SCTLR_ELx_EE    (BIT(25))
#define SCTLR_ELx_IESB  (BIT(21))
#define SCTLR_ELx_WXN   (BIT(19))
#define SCTLR_ELx_ENDB  (BIT(13))
#define SCTLR_ELx_I     (BIT(12))
#define SCTLR_ELx_SA    (BIT(3))
#define SCTLR_ELx_C     (BIT(2))
#define SCTLR_ELx_A     (BIT(1))
#define SCTLR_ELx_M     (BIT(0))

#define SCTLR_EL1_EPAN (BIT(57))
#define SCTLR_EL1_ATA0 (BIT(42))

#define SCTLR_EL1_BT1     (BIT(36))
#define SCTLR_EL1_BT0     (BIT(35))
#define SCTLR_EL1_UCI     (BIT(26))
#define SCTLR_EL1_E0E     (BIT(24))
#define SCTLR_EL1_SPAN    (BIT(23))
#define SCTLR_EL1_NTWE    (BIT(18))
#define SCTLR_EL1_NTWI    (BIT(16))
#define SCTLR_EL1_UCT     (BIT(15))
#define SCTLR_EL1_DZE     (BIT(14))
#define SCTLR_EL1_UMA     (BIT(9))
#define SCTLR_EL1_SED     (BIT(8))
#define SCTLR_EL1_ITD     (BIT(7))
#define SCTLR_EL1_CP15BEN (BIT(5))
#define SCTLR_EL1_SA0     (BIT(4))

#define SCTLR_EL1_RES1 \
    ((BIT(11)) | (BIT(20)) | (BIT(22)) | (BIT(28)) | (BIT(29)))

#define INIT_SCTLR_EL1_MMU_ON                                             \
    (SCTLR_ELx_M | SCTLR_ELx_C | SCTLR_ELx_SA | SCTLR_EL1_SA0 |           \
     SCTLR_EL1_SED | SCTLR_ELx_I | SCTLR_EL1_DZE | SCTLR_EL1_UCT |        \
     SCTLR_EL1_NTWE | SCTLR_ELx_IESB | SCTLR_EL1_SPAN | SCTLR_ELx_ITFSB | \
     SCTLR_ELx_ATA | SCTLR_EL1_ATA0 | SCTLR_EL1_UCI | SCTLR_EL1_EPAN |    \
     SCTLR_EL1_RES1)

#define MAIR_ATTR_DEVICE_nGnRnE UL(0x00)
#define MAIR_ATTR_DEVICE_nGnRE  UL(0x04)
#define MAIR_ATTR_NORMAL_NC     UL(0x44)
#define MAIR_ATTR_NORMAL_TAGGED UL(0xf0)
#define MAIR_ATTR_NORMAL        UL(0xff)
#define MAIR_ATTR_MASK          UL(0xff)

#define MAIR_ATTRIDX(attr, idx) ((attr) << ((idx)*8))

#define CPACR_EL1_FPEN_EL1EN (BIT(20)) /* enable EL1 access */
#define CPACR_EL1_FPEN_EL0EN (BIT(21)) /* enable EL0 access, if EL1EN set */
#define CPACR_EL1_FPEN       (CPACR_EL1_FPEN_EL1EN | CPACR_EL1_FPEN_EL0EN)

/* id_aa64mmfr0 */
#define ID_AA64MMFR0_ECV_SHIFT       60
#define ID_AA64MMFR0_FGT_SHIFT       56
#define ID_AA64MMFR0_EXS_SHIFT       44
#define ID_AA64MMFR0_TGRAN4_2_SHIFT  40
#define ID_AA64MMFR0_TGRAN64_2_SHIFT 36
#define ID_AA64MMFR0_TGRAN16_2_SHIFT 32
#define ID_AA64MMFR0_TGRAN4_SHIFT    28
#define ID_AA64MMFR0_TGRAN64_SHIFT   24
#define ID_AA64MMFR0_TGRAN16_SHIFT   20
#define ID_AA64MMFR0_BIGENDEL0_SHIFT 16
#define ID_AA64MMFR0_SNSMEM_SHIFT    12
#define ID_AA64MMFR0_BIGENDEL_SHIFT  8
#define ID_AA64MMFR0_ASID_SHIFT      4
#define ID_AA64MMFR0_PARANGE_SHIFT   0

#define ID_AA64MMFR0_PARANGE_32  0x0
#define ID_AA64MMFR0_PARANGE_36  0x1
#define ID_AA64MMFR0_PARANGE_40  0x2
#define ID_AA64MMFR0_PARANGE_42  0x3
#define ID_AA64MMFR0_PARANGE_44  0x4
#define ID_AA64MMFR0_PARANGE_48  0x5
#define ID_AA64MMFR0_PARANGE_52  0x6
#define ID_AA64MMFR0_PARANGE_MAX ID_AA64MMFR0_PARANGE_48

#define SYS_MIDR_EL1   sys_reg(3, 0, 0, 0, 0)
#define SYS_MPIDR_EL1  sys_reg(3, 0, 0, 0, 5)
#define SYS_REVIDR_EL1 sys_reg(3, 0, 0, 0, 6)

#define __DEFINE_MRS_MSR_S_REGNUM                                              \
    "	.irp	"                                                                   \
    "num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25," \
    "26,27,28,29,30\n"                                                         \
    "	.equ	.L__reg_num_x\\num, \\num\n"                                        \
    "	.endr\n"                                                                 \
    "	.equ	.L__reg_num_xzr, 31\n"

#define DEFINE_MRS_S                            \
    __DEFINE_MRS_MSR_S_REGNUM                   \
    "	.macro	mrs_s, rt, sreg\n" __emit_inst( \
        0xd5200000 | (\\sreg) | (.L__reg_num_\\rt)) "	.endm\n"

#define DEFINE_MSR_S                            \
    __DEFINE_MRS_MSR_S_REGNUM                   \
    "	.macro	msr_s, sreg, rt\n" __emit_inst( \
        0xd5000000 | (\\sreg) | (.L__reg_num_\\rt)) "	.endm\n"

#define UNDEFINE_MRS_S "	.purgem	mrs_s\n"

#define UNDEFINE_MSR_S "	.purgem	msr_s\n"

#define __mrs_s(v, r) \
    DEFINE_MRS_S      \
    "	mrs_s " v ", " #r "\n" UNDEFINE_MRS_S

#define __msr_s(r, v) \
    DEFINE_MSR_S      \
    "	msr_s " #r ", " v "\n" UNDEFINE_MSR_S

#define read_sysreg(r)                             \
    ({                                             \
        u64 __val;                                 \
        asm volatile("mrs %0, " #r : "=r"(__val)); \
        __val;                                     \
    })

#define write_sysreg(v, r)                               \
    do {                                                 \
        u64 __val = (u64)(v);                            \
        asm volatile("msr " #r ", %x0" : : "rZ"(__val)); \
    } while (0)

#define read_sysreg_s(r)                              \
    ({                                                \
        u64 __val;                                    \
        asm volatile(__mrs_s("%0", r) : "=r"(__val)); \
        __val;                                        \
    })

#define write_sysreg_s(v, r)                             \
    do {                                                 \
        u64 __val = (u64)(v);                            \
        asm volatile(__msr_s(r, "%x0") : : "rZ"(__val)); \
    } while (0)

#endif
