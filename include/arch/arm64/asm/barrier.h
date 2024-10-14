#ifndef _ASM_BARRIER_H_
#define _ASM_BARRIER_H_

#define cmb() __asm__ __volatile__("" ::: "memory")

#define sev() asm volatile("sev" : : : "memory")
#define wfe() asm volatile("wfe" : : : "memory")
#define wfi() asm volatile("wfi" : : : "memory")

#define isb()    asm volatile("isb" : : : "memory")
#define dmb(opt) asm volatile("dmb " #opt : : : "memory")
#define dsb(opt) asm volatile("dsb " #opt : : : "memory")

#define mb()
#define rmb()
#define wmb()

#endif
