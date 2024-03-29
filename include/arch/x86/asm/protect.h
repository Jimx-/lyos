/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _PROTECT_H_
#define _PROTECT_H_

#include <lyos/config.h>
#include <asm/page.h>

#ifndef __ASSEMBLY__

#include <asm/cpulocals.h>

/* Segment descriptor */
struct descriptor /* 8 bytes */
{
    u16 limit_low;       /* Limit */
    u16 base_low;        /* Base */
    u8 base_mid;         /* Base */
    u8 attr1;            /* P(1) DPL(2) DT(1) TYPE(4) */
    u8 limit_high_attr2; /* G(1) D(1) 0(1) AVL(1) LimitHigh(4) */
    u8 base_high;        /* Base */
} __attribute__((packed));

struct ldttss_descriptor {
    u16 limit_low;       /* Limit */
    u16 base0;           /* Base */
    u8 base1;            /* Base */
    u8 attr1;            /* P(1) DPL(2) DT(1) TYPE(4) */
    u8 limit_high_attr2; /* G(1) D(1) 0(1) AVL(1) LimitHigh(4) */
    u8 base2;            /* Base */
#ifdef CONFIG_X86_64
    u32 base3;
    u32 zero0;
#endif
} __attribute__((packed));

#define reassembly(high, high_shift, mid, mid_shift, low) \
    (((high) << (high_shift)) + ((mid) << (mid_shift)) + (low))

/* Gate descriptor */
struct gate {
    u16 offset_low;    /* Offset Low */
    u16 selector;      /* Selector */
    u8 dcount;         /*  */
    u8 attr;           /* P(1) DPL(2) DT(1) TYPE(4) */
    u16 offset_middle; /* Offset High */
#ifdef CONFIG_X86_64
    u32 offset_high;
    u32 reserved;
#endif
} __attribute__((packed));

#ifdef CONFIG_X86_32 /* 32-bit: */
struct tss {
    u32 backlink;
    u32 esp0; /* stack pointer to use during interrupt */
    u32 ss0;  /*   "   segment  "  "    "        "     */
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 flags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u16 trap;
    u16 iobase;
    /*u8	iomap[2];*/
} __attribute__((packed));
#else /* 64-bit: */
struct tss {
    u32 reserved1;
    u64 sp0;
    u64 sp1;
    u64 sp2;
    u64 reserved2;
    u64 ist[7];
    u32 reserved3;
    u32 reserved4;
    u16 reserved5;
    u16 iobase;
} __attribute__((packed));
#endif

#endif /* !__ASSEMBLY__ */

/* GDT */
/* desciptor index */
#define INDEX_DUMMY 0

#ifdef CONFIG_X86_32 /* 32-bit: */
#define INDEX_KERNEL_C  1
#define INDEX_KERNEL_RW 2
#define INDEX_USER_C    3
#define INDEX_USER_RW   4
#define INDEX_LDT       5
#define INDEX_CPULOCALS 6
#define INDEX_TSS       7
#define INDEX_TLS_MIN   8
#define INDEX_TLS_MAX   (INDEX_TLS_MIN + GDT_TLS_ENTRIES - 1)
#else /* 64-bit: */
#define INDEX_KERNEL32_C 1
#define INDEX_KERNEL_C   2
#define INDEX_KERNEL_RW  3
#define INDEX_USER32_C   4
#define INDEX_USER_RW    5
#define INDEX_USER_C     6
#define INDEX_LDT        8
#define INDEX_TSS        10
#define INDEX_TLS_MIN    12
#define INDEX_TLS_MAX    14
#endif

#define GDT_TLS_ENTRIES 3

/* selector */
#define SEG_SELECTOR(i)    (i * 8)
#define SELECTOR_DUMMY     0
#define SELECTOR_KERNEL_C  SEG_SELECTOR(INDEX_KERNEL_C)
#define SELECTOR_KERNEL_RW SEG_SELECTOR(INDEX_KERNEL_RW)
#define SELECTOR_USER_C    SEG_SELECTOR(INDEX_USER_C)
#define SELECTOR_USER_RW   SEG_SELECTOR(INDEX_USER_RW)
#define SELECTOR_LDT       SEG_SELECTOR(INDEX_LDT)
#define SELECTOR_TSS       SEG_SELECTOR(INDEX_TSS)

#define SELECTOR_KERNEL_CS SELECTOR_KERNEL_C
#define SELECTOR_KERNEL_DS SELECTOR_KERNEL_RW
#define SELECTOR_USER_CS   SELECTOR_USER_C
#define SELECTOR_USER_DS   SELECTOR_USER_RW

#ifdef CONFIG_X86_32
#define SELECTOR_CPULOCALS SEG_SELECTOR(INDEX_CPULOCALS)
#else
#define SELECTOR_KERNEL32_C  SEG_SELECTOR(INDEX_KERNEL32_C)
#define SELECTOR_KERNEL32_CS SELECTOR_KERNEL32_C
#define SELECTOR_USER32_C    SEG_SELECTOR(INDEX_USER32_C)
#define SELECTOR_USER32_CS   SELECTOR_USER32_C
#endif

/* 描述符类型值说明 */
#define DA_L           0x2000 /* Long-mode code */
#define DA_32          0x4000 /* 32 位段				*/
#define DA_LIMIT_4K    0x8000 /* 段界限粒度为 4K 字节			*/
#define LIMIT_4K_SHIFT 12
#define DA_DPL0        0x00 /* DPL = 0				*/
#define DA_DPL1        0x20 /* DPL = 1				*/
#define DA_DPL2        0x40 /* DPL = 2				*/
#define DA_DPL3        0x60 /* DPL = 3				*/
/* 存储段描述符类型值说明 */
#define DA_DR   0x90 /* 存在的只读数据段类型值		*/
#define DA_DRW  0x92 /* 存在的可读写数据段属性值		*/
#define DA_DRWA 0x93 /* 存在的已访问可读写数据段类型值	*/
#define DA_C    0x98 /* 存在的只执行代码段属性值		*/
#define DA_CR   0x9A /* 存在的可执行可读代码段属性值		*/
#define DA_CCO  0x9C /* 存在的只执行一致代码段属性值		*/
#define DA_CCOR 0x9E /* 存在的可执行可读一致代码段属性值	*/
/* 系统段描述符类型值说明 */
#define DA_LDT      0x82 /* 局部描述符表段类型值			*/
#define DA_TaskGate 0x85 /* 任务门类型值				*/
#define DA_386TSS   0x89 /* 可用 386 任务状态段类型值		*/
#define DA_386CGate 0x8C /* 386 调用门类型值			*/
#define DA_386IGate 0x8E /* 386 中断门类型值			*/
#define DA_386TGate 0x8F /* 386 陷阱门类型值			*/

/* 选择子类型值说明 */
/* 其中, SA_ : Selector Attribute */
#define SA_RPL_MASK 0xFFFC
#define SA_RPL0     0
#define SA_RPL1     1
#define SA_RPL2     2
#define SA_RPL3     3

#define SA_TI_MASK 0xFFFB
#define SA_TIG     0
#define SA_TIL     4

/* 权限 */
#define PRIVILEGE_KRNL 0
#define PRIVILEGE_TASK 1
#define PRIVILEGE_USER 3
/* RPL */
#define RPL_KRNL SA_RPL0
#define RPL_TASK SA_RPL1
#define RPL_USER SA_RPL3

/* 中断向量 */
#define INT_VECTOR_DIVIDE       0x0
#define INT_VECTOR_DEBUG        0x1
#define INT_VECTOR_NMI          0x2
#define INT_VECTOR_BREAKPOINT   0x3
#define INT_VECTOR_OVERFLOW     0x4
#define INT_VECTOR_BOUNDS       0x5
#define INT_VECTOR_INVAL_OP     0x6
#define INT_VECTOR_COPROC_NOT   0x7
#define INT_VECTOR_DOUBLE_FAULT 0x8
#define INT_VECTOR_COPROC_SEG   0x9
#define INT_VECTOR_INVAL_TSS    0xA
#define INT_VECTOR_SEG_NOT      0xB
#define INT_VECTOR_STACK_FAULT  0xC
#define INT_VECTOR_PROTECTION   0xD
#define INT_VECTOR_PAGE_FAULT   0xE
#define INT_VECTOR_COPROC_ERR   0x10

/* 中断向量 */
#define INT_VECTOR_IRQ0 0x20
#define INT_VECTOR_IRQ8 0x28

/* 系统调用 */
#define INT_VECTOR_SYS_CALL 0x90

#define GDT_SIZE 128

#ifndef __ASSEMBLY__

struct gdt_page {
    struct descriptor gdt[GDT_SIZE];
} __attribute__((aligned(ARCH_PG_SIZE)));

DECLARE_CPULOCAL(struct gdt_page, gdt_page)
__attribute__((aligned(ARCH_PG_SIZE)));

static inline int descriptor_empty(const void* ptr)
{
    const u32* desc = ptr;
    return !(desc[0] | desc[1]);
}

static inline struct descriptor* get_cpu_gdt(unsigned int cpu)
{
    return get_cpu_var(cpu, gdt_page).gdt;
}

/* protect.c */
void init_prot();
void init_desc(struct descriptor* p_desc, unsigned long base, u32 limit,
               u16 attribute);
void load_direct_gdt(unsigned int cpu);
void load_prot_selectors(unsigned int cpu);
int init_tss(unsigned cpu, void* kernel_stack);
void init_idt();
void init_idt_desc(unsigned char vector, u8 desc_type, int_handler handler,
                   unsigned char privilege);

void reload_idt();

#endif /* !__ASSEMBLY__ */

#endif /* _PROTECT_H_ */
