#ifndef _RISCV_MCONTEXT_H_
#define _RISCV_MCONTEXT_H_

#define _NGREG 32
typedef unsigned long __greg_t;

#define _REG_X1  0
#define _REG_X2  1
#define _REG_X3  2
#define _REG_X4  3
#define _REG_X5  4
#define _REG_X6  5
#define _REG_X7  6
#define _REG_X8  7
#define _REG_X9  8
#define _REG_X10 9
#define _REG_X11 10
#define _REG_X12 11
#define _REG_X13 12
#define _REG_X14 13
#define _REG_X15 14
#define _REG_X16 15
#define _REG_X17 16
#define _REG_X18 17
#define _REG_X19 18
#define _REG_X20 19
#define _REG_X21 20
#define _REG_X22 21
#define _REG_X23 22
#define _REG_X24 23
#define _REG_X25 24
#define _REG_X26 25
#define _REG_X27 26
#define _REG_X28 27
#define _REG_X29 28
#define _REG_X30 29
#define _REG_X31 30
#define _REG_PC  31

#define _REG_RA _REG_X1
#define _REG_SP _REG_X2
#define _REG_GP _REG_X3
#define _REG_TP _REG_X4
#define _REG_S0 _REG_X8
#define _REG_RV _REG_X10
#define _REG_A0 _REG_X10

typedef struct {
    __greg_t gregs[_NGREG];
} mcontext_t;

#ifndef _UC_MACHINE_SP
#define _UC_MACHINE_SP(uc) ((uc)->uc_mcontext.gregs[_REG_SP])
#endif

#define _UC_MACHINE_PC(uc)         ((uc)->uc_mcontext.gregs[_REG_PC])
#define _UC_MACHINE_SET_PC(uc, pc) _UC_MACHINE_PC(uc) = (pc)

#define _UC_MACHINE_STACK(uc)         ((uc)->uc_mcontext.gregs[_REG_SP])
#define _UC_MACHINE_SET_STACK(uc, sp) _UC_MACHINE_STACK(uc) = sp

#define _UC_MACHINE_INTRV(uc) ((uc)->uc_mcontext.gregs[_REG_RV])

#endif
