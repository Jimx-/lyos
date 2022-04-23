.globl arch_spinlock_lock
.globl arch_spinlock_unlock

arch_spinlock_lock:
    mov     w2, #1
    sevl
1:  wfe
2:  ldaxr   w1, [x0]
    cbnz    w1, 1b
    stxr    w1, w2, [x0]
    cbnz    w1, 2b
    ret

arch_spinlock_unlock:
    stlr    wzr, [x0]
    ret
