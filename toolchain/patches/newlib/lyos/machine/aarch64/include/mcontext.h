#ifndef _AARCH64_MCONTEXT_H_
#define _AARCH64_MCONTEXT_H_

#define _NGREG 23
typedef unsigned long __greg_t;

#define _REG_X0    0
#define _REG_X1    1
#define _REG_X2    2
#define _REG_X3    3
#define _REG_X4    4
#define _REG_X5    5
#define _REG_X6    6
#define _REG_X7    7
#define _REG_X8    8
#define _REG_X9    9
#define _REG_X10   10
#define _REG_X11   11
#define _REG_X12   12
#define _REG_X13   13
#define _REG_X14   14
#define _REG_X15   15
#define _REG_X16   16
#define _REG_X17   17
#define _REG_X18   18
#define _REG_X19   19
#define _REG_X20   20
#define _REG_X21   21
#define _REG_X22   22
#define _REG_X23   23
#define _REG_X24   24
#define _REG_X25   25
#define _REG_X26   26
#define _REG_X27   27
#define _REG_X28   28
#define _REG_X29   29
#define _REG_X30   30
#define _REG_X31   31
#define _REG_ELR   32
#define _REG_SPSR  33
#define _REG_TPIDR 34

#define _REG_RV _REG_X0
#define _REG_FP _REG_X29
#define _REG_LR _REG_X30
#define _REG_SP _REG_X31
#define _REG_PC _REG_ELR

typedef struct {
    __greg_t gregs[_NGREG];
} mcontext_t;

#ifndef _UC_MACHINE_SP
#define _UC_MACHINE_SP(uc) ((uc)->uc_mcontext.gregs[_REG_SP])
#endif
#define _UC_MACHINE_PC(uc)    ((uc)->uc_mcontext.gregs[_REG_PC])
#define _UC_MACHINE_INTRV(uc) ((uc)->uc_mcontext.gregs[_REG_RV])

#define _UC_MACHINE_SET_PC(uc, pc) _UC_MACHINE_PC(uc) = (pc)

#define _UC_MACHINE_STACK(uc)         ((uc)->uc_mcontext.gregs[_REG_SP])
#define _UC_MACHINE_SET_STACK(uc, sp) _UC_MACHINE_STACK(uc) = (sp)

#endif
