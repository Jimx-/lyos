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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include <asm/protect.h>
#include "lyos/const.h"
#include "lyos/proc.h"
#include "string.h"
#include <errno.h>
#include <signal.h>
#include "lyos/global.h"
#include "lyos/proto.h"
#include <asm/const.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <lyos/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/vm.h>

extern u32 StackTop;

DEFINE_CPULOCAL(struct gdt_page, gdt_page)
__attribute__((aligned(ARCH_PG_SIZE)));

u32 percpu_kstack[CONFIG_SMP_MAX_CPUS];

int syscall_style = 0;

struct exception_info {
    char* msg;
    int signo;
};

struct exception_frame {
    reg_t vec_no;   /* which interrupt vector was triggered */
    reg_t err_code; /* zero if no exception does not push err code */
    reg_t eip;
    reg_t cs;
    reg_t eflags;
};

static struct exception_info err_description[] = {
    {"#DE Divide Error", SIGFPE},
    {"#DB RESERVED", SIGTRAP},
    {"—  NMI Interrupt", SIGBUS},
    {"#BP Breakpoint", SIGTRAP},
    {"#OF Overflow", SIGFPE},
    {"#BR BOUND Range Exceeded", SIGFPE},
    {"#UD Invalid Opcode (Undefined Opcode)", SIGILL},
    {"#NM Device Not Available (No Math Coprocessor)", SIGFPE},
    {"#DF Double Fault", SIGSEGV},
    {"    Coprocessor Segment Overrun (reserved)", SIGSEGV},
    {"#TS Invalid TSS", SIGSEGV},
    {"#NP Segment Not Present", SIGSEGV},
    {"#SS Stack-Segment Fault", SIGSEGV},
    {"#GP General Protection", SIGSEGV},
    {"#PF Page Fault", SIGSEGV},
    {"—  (Intel reserved. Do not use.)", SIGILL},
    {"#MF x87 FPU Floating-Point Error (Math Fault)", SIGFPE},
    {"#AC Alignment Check", SIGBUS},
    {"#MC Machine Check", SIGBUS},
    {"#XF SIMD Floating-Point Exception", SIGFPE},
};

//#define PROTECT_DEBUG

/* 本文件内函数声明 */
void init_idt_desc(unsigned char vector, u8 desc_type, int_handler handler,
                   unsigned char privilege);

/* 中断处理函数 */
void divide_error();
void single_step_exception();
void nmi();
void breakpoint_exception();
void overflow();
void bounds_check();
void inval_opcode();
void copr_not_available();
void double_fault();
void copr_seg_overrun();
void inval_tss();
void segment_not_present();
void stack_exception();
void general_protection();
void page_fault();
void copr_error();
void hwint00();
void hwint01();
void hwint02();
void hwint03();
void hwint04();
void hwint05();
void hwint06();
void hwint07();
void hwint08();
void hwint09();
void hwint10();
void hwint11();
void hwint12();
void hwint13();
void hwint14();
void hwint15();

void phys_copy_fault();
void phys_copy_fault_in_kernel();
void copy_user_message_end();
void copy_user_message_fault();
static void page_fault_handler(int in_kernel, struct exception_frame* frame);

void init_idt();

/*======================================================================*
                            init_prot
 *----------------------------------------------------------------------*
 初始化 IDT
 *======================================================================*/
void init_prot()
{
    struct descriptor* gdt;

    if (_cpufeature(_CPUF_I386_SYSENTER)) syscall_style |= SST_INTEL_SYSENTER;
#ifdef CONFIG_64BIT
    if (_cpufeature(_CPUF_I386_SYSCALL)) syscall_style |= SST_AMD_SYSCALL;
#endif

    /* setup gdt */
    gdt = get_cpu_gdt(booting_cpu);
    init_desc(&gdt[0], 0, 0, 0);
    init_desc(&gdt[INDEX_KERNEL_C], 0, 0xfffff, DA_CR | DA_32 | DA_LIMIT_4K);
    init_desc(&gdt[INDEX_KERNEL_RW], 0, 0xfffff, DA_DRW | DA_32 | DA_LIMIT_4K);
    init_desc(&gdt[INDEX_USER_C], 0, 0xfffff,
              DA_32 | DA_LIMIT_4K | DA_C | PRIVILEGE_USER << 5);
    init_desc(&gdt[INDEX_USER_RW], 0, 0xfffff,
              DA_32 | DA_LIMIT_4K | DA_DRW | PRIVILEGE_USER << 5);
    init_desc(&gdt[INDEX_LDT], 0, 0, DA_LDT);

    init_8259A();

    init_idt();

    init_tss(0, (u32)&StackTop);

    load_prot_selectors(booting_cpu);
}

void load_direct_gdt(unsigned int cpu)
{
    u8 gdt_ptr[6]; /* 0~15:Limit  16~47:Base */

    u16* p_gdt_limit = (u16*)(&gdt_ptr[0]);
    u32* p_gdt_base = (u32*)(&gdt_ptr[2]);
    *p_gdt_limit = GDT_SIZE * sizeof(struct descriptor) - 1;
    *p_gdt_base = (u32)get_cpu_gdt(cpu);

    x86_lgdt((u8*)&gdt_ptr);
}

void load_prot_selectors(unsigned int cpu)
{
    u8 idt_ptr[6]; /* 0~15:Limit  16~47:Base */

    u16* p_idt_limit = (u16*)(&idt_ptr[0]);
    u32* p_idt_base = (u32*)(&idt_ptr[2]);
    *p_idt_limit = IDT_SIZE * sizeof(struct gate) - 1;
    *p_idt_base = (u32)&idt;

    load_direct_gdt(cpu);
    x86_lidt((u8*)&idt_ptr);
    x86_lldt(SELECTOR_LDT);
    x86_ltr(SELECTOR_TSS);

    x86_load_kerncs();
    x86_load_ds(SELECTOR_KERNEL_DS);
    x86_load_es(SELECTOR_KERNEL_DS);
    x86_load_fs(SELECTOR_CPULOCALS);
    x86_load_gs(SELECTOR_KERNEL_DS);
    x86_load_ss(SELECTOR_KERNEL_DS);
}

void init_idt()
{
    /* 全部初始化成中断门(没有陷阱门) */
    init_idt_desc(INT_VECTOR_DIVIDE, DA_386IGate, divide_error, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_DEBUG, DA_386IGate, single_step_exception,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_NMI, DA_386IGate, nmi, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_BREAKPOINT, DA_386IGate, breakpoint_exception,
                  PRIVILEGE_USER);

    init_idt_desc(INT_VECTOR_OVERFLOW, DA_386IGate, overflow, PRIVILEGE_USER);

    init_idt_desc(INT_VECTOR_BOUNDS, DA_386IGate, bounds_check, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_INVAL_OP, DA_386IGate, inval_opcode,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_COPROC_NOT, DA_386IGate, copr_not_available,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_DOUBLE_FAULT, DA_386IGate, double_fault,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_COPROC_SEG, DA_386IGate, copr_seg_overrun,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_INVAL_TSS, DA_386IGate, inval_tss, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_SEG_NOT, DA_386IGate, segment_not_present,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_STACK_FAULT, DA_386IGate, stack_exception,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_PROTECTION, DA_386IGate, general_protection,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_PAGE_FAULT, DA_386IGate, page_fault,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_COPROC_ERR, DA_386IGate, copr_error,
                  PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 0, DA_386IGate, hwint00, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 1, DA_386IGate, hwint01, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 2, DA_386IGate, hwint02, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 3, DA_386IGate, hwint03, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 4, DA_386IGate, hwint04, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 5, DA_386IGate, hwint05, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 6, DA_386IGate, hwint06, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ0 + 7, DA_386IGate, hwint07, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 0, DA_386IGate, hwint08, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 1, DA_386IGate, hwint09, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 2, DA_386IGate, hwint10, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 3, DA_386IGate, hwint11, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 4, DA_386IGate, hwint12, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 5, DA_386IGate, hwint13, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 6, DA_386IGate, hwint14, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_IRQ8 + 7, DA_386IGate, hwint15, PRIVILEGE_KRNL);

    init_idt_desc(INT_VECTOR_SYS_CALL, DA_386IGate, sys_call, PRIVILEGE_USER);
}

/*======================================================================*
                             init_idt_desc
 *----------------------------------------------------------------------*
 初始化 386 中断门
 *======================================================================*/
void init_idt_desc(unsigned char vector, u8 desc_type, int_handler handler,
                   unsigned char privilege)
{
    struct gate* p_gate = &idt[vector];
    u32 base = (u32)handler;
    p_gate->offset_low = base & 0xFFFF;
    p_gate->selector = SELECTOR_KERNEL_CS;
    p_gate->dcount = 0;
    p_gate->attr = desc_type | (privilege << 5);
    p_gate->offset_high = (base >> 16) & 0xFFFF;
}

void reload_idt()
{
    u8 idt_ptr[6];

    u16* p_idt_limit = (u16*)(&idt_ptr[0]);
    u32* p_idt_base = (u32*)(&idt_ptr[2]);
    *p_idt_limit = IDT_SIZE * sizeof(struct gate) - 1;
    *p_idt_base = (u32)&idt;

    x86_lidt((u8*)&idt_ptr);
}

void sys_call_syscall_cpu0();
void sys_call_syscall_cpu1();
void sys_call_syscall_cpu2();
void sys_call_syscall_cpu3();
void sys_call_syscall_cpu4();
void sys_call_syscall_cpu5();
void sys_call_syscall_cpu6();
void sys_call_syscall_cpu7();

int init_tss(unsigned cpu, unsigned kernel_stack)
{
    struct tss* t = &tss[cpu];
    struct descriptor* gdt = get_cpu_gdt(cpu);

    /* Fill the TSS descriptor in GDT */
    memset(t, 0, sizeof(struct tss));
    t->ss0 = SELECTOR_KERNEL_DS;
    t->cs = SELECTOR_KERNEL_CS;
    percpu_kstack[cpu] = t->esp0 = kernel_stack - X86_STACK_TOP_RESERVED;
    init_desc(&gdt[INDEX_CPULOCALS], cpulocals_offset(cpu), 0xfffff,
              DA_DRW | DA_32 | DA_LIMIT_4K);
    init_desc(&gdt[INDEX_TSS], (u32)t, sizeof(struct tss) - 1, DA_386TSS);
    t->iobase = sizeof(struct tss); /* No IO permission bitmap */

    /* set cpuid */
    *((u32*)(t->esp0 + sizeof(u32))) = cpu;

    if (syscall_style & SST_INTEL_SYSENTER) {
        ia32_write_msr(INTEL_MSR_SYSENTER_CS, 0, SELECTOR_KERNEL_CS);
        ia32_write_msr(INTEL_MSR_SYSENTER_ESP, 0, t->esp0);
        ia32_write_msr(INTEL_MSR_SYSENTER_EIP, 0, (u32)sys_call_sysenter);
    }

    if (syscall_style & SST_AMD_SYSCALL) {
        u32 msr_lo, msr_hi;

        /* enable SYSCALL in EFER */
        ia32_read_msr(AMD_MSR_EFER, &msr_hi, &msr_lo);
        msr_lo |= AMD_EFER_SCE;
        ia32_write_msr(AMD_MSR_EFER, msr_hi, msr_lo);

#define set_star(forcpu)                                   \
    if (forcpu == cpu) {                                   \
        ia32_write_msr(AMD_MSR_STAR,                       \
                       ((u32)SELECTOR_USER_CS << 16) |     \
                           (u32)SELECTOR_KERNEL_CS,        \
                       (u32)sys_call_syscall_cpu##forcpu); \
    }

        set_star(0);
        set_star(1);
        set_star(2);
        set_star(3);
        set_star(4);
        set_star(5);
        set_star(6);
        set_star(7);
    }

    return SELECTOR_TSS;
}

void load_tls(struct proc* p, unsigned int cpu)
{
    struct descriptor* gdt = get_cpu_gdt(cpu);
    int i;

    for (i = 0; i < GDT_TLS_ENTRIES; i++) {
        gdt[INDEX_TLS_MIN + i] = p->seg.tls_array[i];
    }
}

/*======================================================================*
                           init_descriptor
 *----------------------------------------------------------------------*
 初始化段描述符
 *======================================================================*/
void init_desc(struct descriptor* p_desc, u32 base, u32 limit, u16 attribute)
{
    p_desc->limit_low = limit & 0x0FFFF;     /* 段界限 1		(2 字节) */
    p_desc->base_low = base & 0x0FFFF;       /* 段基址 1		(2 字节) */
    p_desc->base_mid = (base >> 16) & 0x0FF; /* 段基址 2		(1 字节) */
    p_desc->attr1 = attribute & 0xFF;        /* 属性 1 */
    p_desc->limit_high_attr2 =
        ((limit >> 16) & 0x0F) |
        ((attribute >> 8) & 0xF0);            /* 段界限 2 + 属性 2 */
    p_desc->base_high = (base >> 24) & 0x0FF; /* 段基址 3		(1 字节) */
}

static void print_stacktrace(struct proc* p)
{
    unsigned long bp, hbp, pc;
    int retval;

    bp = p->regs.ebp;

    printk("proc %d: pc=0x%x bp=0x%x ", p->endpoint, p->regs.eip, bp);

    while (bp) {
        retval = data_vir_copy(KERNEL, &pc, p->endpoint,
                               &((unsigned long*)bp)[1], sizeof(pc));
        if (retval != OK) break;

        retval = data_vir_copy(KERNEL, &hbp, p->endpoint,
                               &((unsigned long*)bp)[0], sizeof(hbp));
        if (retval != OK) break;

        printk("0x%lx ", (unsigned long)pc);

        if (hbp != 0 && hbp <= bp) {
            pc = -1;
            printk("0x%lx ", (unsigned long)pc);
            break;
        }
        bp = hbp;
    }

    printk("\n");
}

/**
 * <Ring 0> Handle page fault.
 */
static void page_fault_handler(int in_kernel, struct exception_frame* frame)
{
    reg_t pfla = read_cr2();

#ifdef PROTECT_DEBUG
    if (frame->err_code & ARCH_PG_PRESENT) {
        printk("  Protection violation ");
    } else {
        printk("  Page not present ");
    }

    if (frame->err_code & ARCH_PG_RW) {
        printk("caused by write access ");
    } else {
        printk("caused by read access ");
    }

    if (frame->err_code & ARCH_PG_USER) {
        printk("in user mode");
    } else {
        printk("in supervisor mode");
    }

    printk("\n  CR2(PFLA): 0x%x, CR3: 0x%x, EIP: 0x%x\n", pfla, read_cr3(),
           frame->eip);
#endif

    struct proc* fault_proc = get_cpulocal_var(proc_ptr);

    int in_phys_copy = (frame->eip >= (uintptr_t)phys_copy) &&
                       (frame->eip < (uintptr_t)phys_copy_fault);
    int in_phys_set = 0; /* not implemented */

    if ((in_kernel || is_kerntaske(fault_proc->endpoint)) &&
        (in_phys_copy || in_phys_set)) {
        if (in_kernel) {
            if (in_phys_copy) {
                frame->eip = (u32)phys_copy_fault_in_kernel;
            }
        } else {
            fault_proc->regs.eip = (u32)phys_copy_fault;
            fault_proc->regs.eax = pfla;
        }

        return;
    }

    if (in_kernel) {
        panic("unhandled page fault in kernel, eip: 0x%x, cr2: 0x%x",
              frame->eip, pfla);
    }

    if (fault_proc->endpoint == TASK_MM) {
        panic("unhandled page fault in MM, eip: 0x%x, cr2: 0x%x", frame->eip,
              pfla);
    }

    int fault_flags = 0;
    if (I386_PF_WRITE(frame->err_code))
        fault_flags |= FAULT_FLAG_WRITE;
    else if (I386_PF_INST(frame->err_code))
        fault_flags |= FAULT_FLAG_INSTRUCTION;

    if (I386_PF_USER(frame->err_code)) fault_flags |= FAULT_FLAG_USER;

    /* inform MM to handle this page fault */
    MESSAGE msg;
    msg.type = FAULT;
    msg.FAULT_ADDR = (void*)pfla;
    msg.FAULT_PROC = fault_proc->endpoint;
    msg.FAULT_ERRCODE = fault_flags;
    msg.FAULT_PC = (void*)frame->eip;

    msg_send(fault_proc, TASK_MM, &msg, IPCF_FROMKERNEL);

    /* block the process */
    PST_SET(fault_proc, PST_PAGEFAULT);
}

/*======================================================================*
                            exception_handler
 *----------------------------------------------------------------------*
 异常处理
 *======================================================================*/
void exception_handler(int in_kernel, struct exception_frame* frame)
{
    if (frame->vec_no == 7 && !in_kernel) {
        copr_not_available_handler();
        return;
    }

    struct proc* fault_proc = get_cpulocal_var(proc_ptr);

#ifdef PROTECT_DEBUG
    printk("Exception: %s on CPU %d\n", err_description[frame->vec_no].msg,
           cpuid);
    printk("  EFLAGS: %d, CS: %d, EIP: 0x%x, PID: %d(%s)", frame->eflags,
           frame->cs, frame->eip, fault_proc->endpoint, fault_proc->name);

    if (err_code != 0xFFFFFFFF) {
        printk(", Error code: %d\n", frame->err_code);
    } else
        printk("\n");
#endif

    if (frame->vec_no == 2) {
        nmi_watchdog_handler(in_kernel, (void*)frame->eip);
        return;
    }

    if (in_kernel) {
        if (frame->eip >= (uintptr_t)copy_user_message &&
            frame->eip < (uintptr_t)copy_user_message_end) {
            if (frame->vec_no == 14 || frame->vec_no == 13) { /* #PF or #GP */
                frame->eip = (reg_t)copy_user_message_fault;
                return;
            } else
                panic("copy user messsage failed unexpectedly");
        }

        if ((frame->eip >= (uintptr_t)frstor &&
             frame->eip < (uintptr_t)frstor_end) ||
            (frame->eip >= (uintptr_t)fxrstor &&
             frame->eip < (uintptr_t)fxrstor_end)) {
            if (frame->vec_no == 14 || frame->vec_no == 13) { /* #PF or #GP */
                frame->eip = (reg_t)frstor_fault;
                return;
            } else
                panic("frstor failed unexpectedly");
        }
    }

    if (frame->vec_no == 14) {
        page_fault_handler(in_kernel, frame);
        return;
    }

    if (in_kernel) {
        panic("unhandled exception in kernel %d, eip: %x", frame->vec_no,
              frame->eip);
    } else {
        print_stacktrace(fault_proc);
        ksig_proc(fault_proc->endpoint, err_description[frame->vec_no].signo);
    }
}
