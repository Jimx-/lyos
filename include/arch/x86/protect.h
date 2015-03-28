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

#ifndef	_PROTECT_H_
#define	_PROTECT_H_


/* Segment descriptor */
struct descriptor		/* 8 bytes */
{
	u16	limit_low;		/* Limit */
	u16	base_low;		/* Base */
	u8	base_mid;		/* Base */
	u8	attr1;			/* P(1) DPL(2) DT(1) TYPE(4) */
	u8	limit_high_attr2;	/* G(1) D(1) 0(1) AVL(1) LimitHigh(4) */
	u8	base_high;		/* Base */
};

#define	reassembly(high, high_shift, mid, mid_shift, low)	\
	(((high) << (high_shift)) +				\
	 ((mid)  << (mid_shift)) +				\
	 (low))

/* Gate descriptor */
struct gate
{
	u16	offset_low;	/* Offset Low */
	u16	selector;	/* Selector */
	u8	dcount;		/*  */
	u8	attr;		/* P(1) DPL(2) DT(1) TYPE(4) */
	u16	offset_high;	/* Offset High */
};

struct tss {
	u32	backlink;
	u32	esp0;		/* stack pointer to use during interrupt */
	u32	ss0;		/*   "   segment  "  "    "        "     */
	u32	esp1;
	u32	ss1;
	u32	esp2;
	u32	ss2;
	u32	cr3;
	u32	eip;
	u32	flags;
	u32	eax;
	u32	ecx;
	u32	edx;
	u32	ebx;
	u32	esp;
	u32	ebp;
	u32	esi;
	u32	edi;
	u32	es;
	u32	cs;
	u32	ss;
	u32	ds;
	u32	fs;
	u32	gs;
	u32	ldt;
	u16	trap;
	u16	iobase;	
	/*u8	iomap[2];*/
};

/* GDT */
/* desciptor index */
#define	INDEX_DUMMY			0
#define	INDEX_KERNEL_C		1
#define	INDEX_KERNEL_RW		2
#define INDEX_USER_C		3
#define INDEX_USER_RW		4
#define	INDEX_LDT 			5
#define INDEX_TSS_FIRST		6
#define	INDEX_TSS(i)		(INDEX_TSS_FIRST + i)

/* selector */
#define SEG_SELECTOR(i)		(i * 8)
#define	SELECTOR_DUMMY		0
#define	SELECTOR_KERNEL_C	SEG_SELECTOR(INDEX_KERNEL_C)
#define	SELECTOR_KERNEL_RW	SEG_SELECTOR(INDEX_KERNEL_RW)
#define	SELECTOR_USER_C		SEG_SELECTOR(INDEX_USER_C)
#define SELECTOR_USER_RW	SEG_SELECTOR(INDEX_USER_RW)
#define SELECTOR_LDT 		SEG_SELECTOR(INDEX_LDT)
#define	SELECTOR_TSS(i)		SEG_SELECTOR(INDEX_TSS(i))

#define	SELECTOR_KERNEL_CS	SELECTOR_KERNEL_C
#define	SELECTOR_KERNEL_DS	SELECTOR_KERNEL_RW
#define	SELECTOR_USER_CS	SELECTOR_USER_C
#define	SELECTOR_USER_DS	SELECTOR_USER_RW

/* 描述符类型值说明 */
#define	DA_32			0x4000	/* 32 位段				*/
#define	DA_LIMIT_4K		0x8000	/* 段界限粒度为 4K 字节			*/
#define	LIMIT_4K_SHIFT		  12
#define	DA_DPL0			0x00	/* DPL = 0				*/
#define	DA_DPL1			0x20	/* DPL = 1				*/
#define	DA_DPL2			0x40	/* DPL = 2				*/
#define	DA_DPL3			0x60	/* DPL = 3				*/
/* 存储段描述符类型值说明 */
#define	DA_DR			0x90	/* 存在的只读数据段类型值		*/
#define	DA_DRW			0x92	/* 存在的可读写数据段属性值		*/
#define	DA_DRWA			0x93	/* 存在的已访问可读写数据段类型值	*/
#define	DA_C			0x98	/* 存在的只执行代码段属性值		*/
#define	DA_CR			0x9A	/* 存在的可执行可读代码段属性值		*/
#define	DA_CCO			0x9C	/* 存在的只执行一致代码段属性值		*/
#define	DA_CCOR			0x9E	/* 存在的可执行可读一致代码段属性值	*/
/* 系统段描述符类型值说明 */
#define	DA_LDT			0x82	/* 局部描述符表段类型值			*/
#define	DA_TaskGate		0x85	/* 任务门类型值				*/
#define	DA_386TSS		0x89	/* 可用 386 任务状态段类型值		*/
#define	DA_386CGate		0x8C	/* 386 调用门类型值			*/
#define	DA_386IGate		0x8E	/* 386 中断门类型值			*/
#define	DA_386TGate		0x8F	/* 386 陷阱门类型值			*/

/* 选择子类型值说明 */
/* 其中, SA_ : Selector Attribute */
#define	SA_RPL_MASK	0xFFFC
#define	SA_RPL0		0
#define	SA_RPL1		1
#define	SA_RPL2		2
#define	SA_RPL3		3

#define	SA_TI_MASK	0xFFFB
#define	SA_TIG		0
#define	SA_TIL		4

/* 权限 */
#define	PRIVILEGE_KRNL	0
#define	PRIVILEGE_TASK	1
#define	PRIVILEGE_USER	3
/* RPL */
#define	RPL_KRNL	SA_RPL0
#define	RPL_TASK	SA_RPL1
#define	RPL_USER	SA_RPL3

/* 中断向量 */
#define	INT_VECTOR_DIVIDE		0x0
#define	INT_VECTOR_DEBUG		0x1
#define	INT_VECTOR_NMI			0x2
#define	INT_VECTOR_BREAKPOINT		0x3
#define	INT_VECTOR_OVERFLOW		0x4
#define	INT_VECTOR_BOUNDS		0x5
#define	INT_VECTOR_INVAL_OP		0x6
#define	INT_VECTOR_COPROC_NOT		0x7
#define	INT_VECTOR_DOUBLE_FAULT		0x8
#define	INT_VECTOR_COPROC_SEG		0x9
#define	INT_VECTOR_INVAL_TSS		0xA
#define	INT_VECTOR_SEG_NOT		0xB
#define	INT_VECTOR_STACK_FAULT		0xC
#define	INT_VECTOR_PROTECTION		0xD
#define	INT_VECTOR_PAGE_FAULT		0xE
#define	INT_VECTOR_COPROC_ERR		0x10

/* 中断向量 */
#define	INT_VECTOR_IRQ0			0x20
#define	INT_VECTOR_IRQ8			0x28

/* 系统调用 */
#define INT_VECTOR_SYS_CALL             0x90

/* 宏 */
/* 线性地址 → 物理地址 */
//#define vir2phys(seg_base, vir)	(u32)(((u32)seg_base) + (u32)(vir))

/* seg:off -> linear addr */
#define makelinear(seg,off) (u32)(((u32)(seg2linear(seg))) + (u32)(off))

/* protect.c */
PUBLIC void	init_prot();
PUBLIC u32	seg2linear(u16 seg);
PUBLIC void	init_desc(struct descriptor * p_desc,
			  u32 base, u32 limit, u16 attribute);
PUBLIC void load_prot_selectors();
PUBLIC int init_tss(unsigned cpu, unsigned kernel_stack);
PUBLIC void init_idt();
PUBLIC void init_idt_desc(unsigned char vector, u8 desc_type, int_handler handler, unsigned char privilege);

PUBLIC void reload_idt();

#endif /* _PROTECT_H_ */
