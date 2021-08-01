#ifndef _ARCH_IO_H_
#define _ARCH_IO_H_

#include <lyos/types.h>

static inline u8 readb(const void* addr) { return *(volatile u8*)addr; }
static inline u32 readl(const void* addr) { return *(volatile u32*)addr; }

static inline void writeb(void* addr, u8 val) { *(volatile u8*)addr = val; }
static inline void writel(void* addr, u32 val) { *(volatile u32*)addr = val; }
#endif
