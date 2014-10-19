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

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "protect.h"
#include "lyos/const.h"
#include "lyos/fs.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/proc.h"
#include "string.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "arch_const.h"
#include "arch_proto.h"
#include "arch_smp.h"
#include <lyos/cpulocals.h>
#include <lyos/cpufeature.h>

extern u32 StackTop;

PUBLIC u32 percpu_kstack[CONFIG_SMP_MAX_CPUS];

PUBLIC int syscall_style = 0; 

#define PROTECT_DEBUG

PUBLIC int msg_send(struct proc* p_to_send, int dest, MESSAGE* m);

/* 本文件内函数声明 */
PRIVATE void init_idt_desc(unsigned char vector, u8 desc_type, int_handler handler, unsigned char privilege);


/* 中断处理函数 */
void	divide_error();
void	single_step_exception();
void	nmi();
void	breakpoint_exception();
void	overflow();
void	bounds_check();
void	inval_opcode();
void	copr_not_available();
void	double_fault();
void	copr_seg_overrun();
void	inval_tss();
void	segment_not_present();
void	stack_exception();
void	general_protection();
void	page_fault();
void	copr_error();
void	hwint00();
void	hwint01();
void	hwint02();
void	hwint03();
void	hwint04();
void	hwint05();
void	hwint06();
void	hwint07();
void	hwint08();
void	hwint09();
void	hwint10();
void	hwint11();
void	hwint12();
void	hwint13();
void	hwint14();
void	hwint15();

PRIVATE void page_fault_handler(int in_kernel, struct exception_frame * frame);

PRIVATE void init_idt();

/*======================================================================*
                            init_prot
 *----------------------------------------------------------------------*
 初始化 IDT
 *======================================================================*/
PUBLIC void init_prot()
{
	if(_cpufeature(_CPUF_I386_SYSENTER))
		syscall_style |= SST_INTEL_SYSENTER;
  	if(_cpufeature(_CPUF_I386_SYSCALL))
		syscall_style |= SST_AMD_SYSCALL;

	init_8259A();

	init_idt();

	init_tss(0, StackTop);

	load_prot_selectors();
}

PUBLIC void load_prot_selectors()
{
	x86_lgdt((u8*)&gdt_ptr);
	x86_lidt((u8*)&idt_ptr);
	x86_lldt(SELECTOR_LDT);
	x86_ltr(SELECTOR_TSS(booting_cpu));

	x86_load_ds(SELECTOR_KERNEL_DS);
	x86_load_es(SELECTOR_KERNEL_DS);
	x86_load_fs(SELECTOR_KERNEL_DS);
	x86_load_gs(SELECTOR_KERNEL_DS);
	x86_load_ss(SELECTOR_KERNEL_DS);
}

PRIVATE void init_idt()
{
	/* 全部初始化成中断门(没有陷阱门) */
	init_idt_desc(INT_VECTOR_DIVIDE,	DA_386IGate,
		      divide_error - KERNEL_VMA,		PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_DEBUG,		DA_386IGate,
		      single_step_exception,	PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_NMI,		DA_386IGate,
		      nmi,			PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_BREAKPOINT,	DA_386IGate,
		      breakpoint_exception,	PRIVILEGE_USER);

	init_idt_desc(INT_VECTOR_OVERFLOW,	DA_386IGate,
		      overflow,			PRIVILEGE_USER);

	init_idt_desc(INT_VECTOR_BOUNDS,	DA_386IGate,
		      bounds_check,		PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_INVAL_OP,	DA_386IGate,
		      inval_opcode,		PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_COPROC_NOT,	DA_386IGate,
		      copr_not_available,	PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_DOUBLE_FAULT,	DA_386IGate,
		      double_fault,		PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_COPROC_SEG,	DA_386IGate,
		      copr_seg_overrun,		PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_INVAL_TSS,	DA_386IGate,
		      inval_tss,		PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_SEG_NOT,	DA_386IGate,
		      segment_not_present,	PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_STACK_FAULT,	DA_386IGate,
		      stack_exception,		PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_PROTECTION,	DA_386IGate,
		      general_protection,	PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_PAGE_FAULT,	DA_386IGate,
		      page_fault,		PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_COPROC_ERR,	DA_386IGate,
		      copr_error,		PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ0 + 0,      DA_386IGate,
                      hwint00,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ0 + 1,      DA_386IGate,
                      hwint01,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ0 + 2,      DA_386IGate,
                      hwint02,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ0 + 3,      DA_386IGate,
                      hwint03,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ0 + 4,      DA_386IGate,
                      hwint04,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ0 + 5,      DA_386IGate,
                      hwint05,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ0 + 6,      DA_386IGate,
                      hwint06,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ0 + 7,      DA_386IGate,
                      hwint07,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ8 + 0,      DA_386IGate,
                      hwint08,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ8 + 1,      DA_386IGate,
                      hwint09,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ8 + 2,      DA_386IGate,
                      hwint10,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ8 + 3,      DA_386IGate,
                      hwint11,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ8 + 4,      DA_386IGate,
                      hwint12,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ8 + 5,      DA_386IGate,
                      hwint13,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ8 + 6,      DA_386IGate,
                      hwint14,                  PRIVILEGE_KRNL);

        init_idt_desc(INT_VECTOR_IRQ8 + 7,      DA_386IGate,
                      hwint15,                  PRIVILEGE_KRNL);

	init_idt_desc(INT_VECTOR_SYS_CALL,	DA_386IGate,
		      sys_call,			PRIVILEGE_USER);
}

/*======================================================================*
                             init_idt_desc
 *----------------------------------------------------------------------*
 初始化 386 中断门
 *======================================================================*/
PUBLIC void init_idt_desc(unsigned char vector, u8 desc_type, int_handler handler, unsigned char privilege)
{
	struct gate * p_gate	= &idt[vector];
	u32 base = (u32)handler;
	p_gate->offset_low	= base & 0xFFFF;
	p_gate->selector	= SELECTOR_KERNEL_CS;
	p_gate->dcount		= 0;
	p_gate->attr		= desc_type | (privilege << 5);
	p_gate->offset_high	= (base >> 16) & 0xFFFF;
}

PUBLIC int init_tss(unsigned cpu, unsigned kernel_stack)
{
	struct tss * t = &tss[cpu];

	/* Fill the TSS descriptor in GDT */
	memset(t, 0, sizeof(struct tss));
	t->ss0	= SELECTOR_KERNEL_DS;
	t->cs = SELECTOR_KERNEL_CS;
	percpu_kstack[cpu] = t->esp0 = kernel_stack - X86_STACK_TOP_RESERVED;
	init_desc(&gdt[INDEX_TSS(cpu)],
		  makelinear(SELECTOR_KERNEL_DS, t),
		  sizeof(struct tss) - 1,
		  DA_386TSS);
	t->iobase = sizeof(struct tss); /* No IO permission bitmap */

	/* set cpuid */
	*((u32*)(t->esp0 + sizeof(u32))) = cpu;

	if (syscall_style & SST_INTEL_SYSENTER) {
		ia32_write_msr(INTEL_MSR_SYSENTER_CS, 0, SELECTOR_KERNEL_CS);
  		ia32_write_msr(INTEL_MSR_SYSENTER_ESP, 0, t->esp0);
  		ia32_write_msr(INTEL_MSR_SYSENTER_EIP, 0, (u32)sys_call_sysenter);	
	}

	return SELECTOR_TSS(cpu);
}

/*======================================================================*
                           seg2phys
 *----------------------------------------------------------------------*
 由段名求绝对地址
 *======================================================================*/
PUBLIC u32 seg2linear(u16 seg)
{
	struct descriptor* p_dest = &gdt[seg >> 3];

	return (p_dest->base_high << 24) | (p_dest->base_mid << 16) | (p_dest->base_low);
}

/*======================================================================*
                           init_descriptor
 *----------------------------------------------------------------------*
 初始化段描述符
 *======================================================================*/
PUBLIC void init_desc(struct descriptor * p_desc, u32 base, u32 limit, u16 attribute)
{
	p_desc->limit_low	= limit & 0x0FFFF;		/* 段界限 1		(2 字节) */
	p_desc->base_low	= base & 0x0FFFF;		/* 段基址 1		(2 字节) */
	p_desc->base_mid	= (base >> 16) & 0x0FF;		/* 段基址 2		(1 字节) */
	p_desc->attr1		= attribute & 0xFF;		/* 属性 1 */
	p_desc->limit_high_attr2= ((limit >> 16) & 0x0F) |
				  ((attribute >> 8) & 0xF0);	/* 段界限 2 + 属性 2 */
	p_desc->base_high	= (base >> 24) & 0x0FF;		/* 段基址 3		(1 字节) */
}

PUBLIC int sys_privctl(MESSAGE * m, struct proc* p)
{
	endpoint_t whom = m->PROC_NR;
	int request = m->REQUEST;
	
	struct proc * target = proc_table + whom;

	switch (request) {
	case PRIVCTL_SET_TASK:
		target->regs.cs = SELECTOR_TASK_CS | RPL_TASK;
		target->regs.ds =
		target->regs.es =
		target->regs.fs =
		target->regs.gs =
		target->regs.ss = SELECTOR_TASK_DS | RPL_TASK;
    	break;
    default:
    	break;
	}

	return 0;
}

/**
 * <Ring 0> Handle page fault.
 */
PRIVATE void page_fault_handler(int in_kernel, struct exception_frame * frame)
{
	int pfla = read_cr2();

#ifdef PROTECT_DEBUG
	if (frame->err_code & PG_PRESENT) {
		printk("  Protection violation ");
	} else {
		printk("  Page not present ");
	}

	if (frame->err_code & PG_RW) {
		printk("caused by write access ");
	} else {
		printk("caused by read access ");
	}

	if (frame->err_code & PG_USER) {
		printk("in user mode");
	} else {
		printk("in supervisor mode");
	}

	printk("\n  CR2(PFLA): 0x%x, CR3: 0x%x\n", pfla, read_cr3());
#endif

	struct proc * fault_proc = get_cpulocal_var(proc_ptr);

	/* inform MM to handle this page fault */
	MESSAGE msg;
	msg.type = FAULT;
	msg.FAULT_NR = frame->vec_no;
	msg.FAULT_ADDR = pfla;
	msg.FAULT_PROC = proc2pid(fault_proc);
	msg.FAULT_ERRCODE = frame->err_code;
	msg.FAULT_STATE = fault_proc->state;

	msg_send(fault_proc, TASK_MM, &msg);
	
	/* block the process */
	PST_SET(fault_proc, PST_RESCUING);
}

/*======================================================================*
                            exception_handler
 *----------------------------------------------------------------------*
 异常处理
 *======================================================================*/
PUBLIC void exception_handler(int in_kernel, struct exception_frame * frame)
{
#ifdef PROTECT_DEBUG
	char err_description[][64] = {	"#DE Divide Error",
					"#DB RESERVED",
					"—  NMI Interrupt",
					"#BP Breakpoint",
					"#OF Overflow",
					"#BR BOUND Range Exceeded",
					"#UD Invalid Opcode (Undefined Opcode)",
					"#NM Device Not Available (No Math Coprocessor)",
					"#DF Double Fault",
					"    Coprocessor Segment Overrun (reserved)",
					"#TS Invalid TSS",
					"#NP Segment Not Present",
					"#SS Stack-Segment Fault",
					"#GP General Protection",
					"#PF Page Fault",
					"—  (Intel reserved. Do not use.)",
					"#MF x87 FPU Floating-Point Error (Math Fault)",
					"#AC Alignment Check",
					"#MC Machine Check",
					"#XF SIMD Floating-Point Exception"
				};

	struct proc * fault_proc = get_cpulocal_var(proc_ptr);
	printk("Exception: %s on CPU %d\n", err_description[frame->vec_no], cpuid);
	printk("  EFLAGS: %d, CS: %d, EIP: 0x%x, PID: %d(%s)", frame->eflags, frame->cs, frame->eip,
		proc2pid(fault_proc), fault_proc->name);

	if(err_code != 0xFFFFFFFF){
		printk(", Error code: %d\n", frame->err_code);
	} else printk("\n");
#endif

	if (in_kernel) {
		panic("unhandled exception in kernel");
	}

	if (frame->vec_no == 14) {
		page_fault_handler(in_kernel, frame);
		return;
	}
}

