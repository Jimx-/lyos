.globl arch_spinlock_lock
.globl arch_spinlock_unlock

arch_spinlock_lock:
    li  a3, 1
    li  a4, 1
1:
    lw  a5, (a0)
    beq a5, a3, 1b
    amoswap.w   a5, a4, (a0)
    fence   r, rw
    bnez    a5, 1b
    ret

arch_spinlock_unlock:
    fence   rw, r
    sw  x0, (a0)
    ret
