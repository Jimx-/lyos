#include <asm/csr.h>

.section .text

.globl copy_user_message
.globl halt_cpu
.globl arch_pause

.globl __trunctfdf2

/* copy a message of 48 bytes */
copy_user_message:
    ret

halt_cpu:
    fence
    csrs sstatus, SR_SIE
    wfi
    csrc sstatus, SR_SIE
    ret

arch_pause:
    ret
	
__trunctfdf2:
    ret
