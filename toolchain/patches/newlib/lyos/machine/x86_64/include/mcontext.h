#ifndef _X86_64_MCONTEXT_H_
#define _X86_64_MCONTEXT_H_

#define _NGREG 23
typedef unsigned long __greg_t;

#define _REG_R8      0
#define _REG_R9      1
#define _REG_R10     2
#define _REG_R11     3
#define _REG_R12     4
#define _REG_R13     5
#define _REG_R14     6
#define _REG_R15     7
#define _REG_RDI     8
#define _REG_RSI     9
#define _REG_RBP     10
#define _REG_RBX     11
#define _REG_RDX     12
#define _REG_RAX     13
#define _REG_RCX     14
#define _REG_RSP     15
#define _REG_RIP     16
#define _REG_EFL     17
#define _REG_CSGSFS  18
#define _REG_ERR     19
#define _REG_TRAPNO  20
#define _REG_OLDMASK 21
#define _REG_CR2     22

typedef struct {
    __greg_t gregs[_NGREG];
} mcontext_t;

#ifndef _UC_MACHINE_SP
#define _UC_MACHINE_SP(uc) ((uc)->uc_mcontext.gregs[_REG_RSP])
#endif
#define _UC_MACHINE_PC(uc)    ((uc)->uc_mcontext.gregs[_REG_RIP])
#define _UC_MACHINE_INTRV(uc) ((uc)->uc_mcontext.gregs[_REG_RAX])

#define _UC_MACHINE_SET_PC(uc, pc) _UC_MACHINE_PC(uc) = (pc)

#define _UC_MACHINE_STACK(uc)          ((uc)->uc_mcontext.gregs[_REG_RSP])
#define _UC_MACHINE_SET_STACK(uc, rsp) _UC_MACHINE_STACK(uc) = (rsp)

#define _UC_MACHINE_RBP(uc)          ((uc)->uc_mcontext.gregs[_REG_RBP])
#define _UC_MACHINE_SET_RBP(uc, rbp) _UC_MACHINE_RBP(uc) = (rbp)

#endif
