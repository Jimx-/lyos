#ifndef _I386_MCONTEXT_H_
#define _I386_MCONTEXT_H_

#define _NGREG 19
typedef int __greg_t;

#define _REG_GS 0
#define _REG_FS 1
#define _REG_ES 2
#define _REG_DS 3
#define _REG_EDI 4
#define _REG_ESI 5
#define _REG_EBP 6
#define _REG_ESP 7
#define _REG_EBX 8
#define _REG_EDX 9
#define _REG_ECX 10
#define _REG_EAX 11
#define _REG_TRAPNO 12
#define _REG_ERR 13
#define _REG_EIP 14
#define _REG_CS 15
#define _REG_EFL 16
#define _REG_UESP 17
#define _REG_SS 18

typedef struct {
    __greg_t gregs[_NGREG];
} mcontext_t;

#ifndef _UC_MACHINE_SP
#define _UC_MACHINE_SP(uc) ((uc)->uc_mcontext.gregs[_REG_UESP])
#endif
#define _UC_MACHINE_PC(uc) ((uc)->uc_mcontext.gregs[_REG_EIP])
#define _UC_MACHINE_INTRV(uc) ((uc)->uc_mcontext.gregs[_REG_EAX])

#define _UC_MACHINE_SET_PC(uc, pc) _UC_MACHINE_PC(uc) = (pc)

#define _UC_MACHINE_STACK(uc) ((uc)->uc_mcontext.gregs[_REG_ESP])
#define _UC_MACHINE_SET_STACK(uc, esp) _UC_MACHINE_STACK(uc) = (esp)

#define _UC_MACHINE_EBP(uc) ((uc)->uc_mcontext.gregs[_REG_EBP])
#define _UC_MACHINE_SET_EBP(uc, ebp) _UC_MACHINE_EBP(uc) = (ebp)

#define _UC_MACHINE_ESI(uc) ((uc)->uc_mcontext.gregs[_REG_ESI])
#define _UC_MACHINE_SET_ESI(uc, esi) _UC_MACHINE_ESI(uc) = (esi)

#endif
