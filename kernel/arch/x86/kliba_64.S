    #include <asm/protect.h>

    .section .text

    .global out_byte
out_byte:
    mov %edi, %edx
    mov %esi, %eax
    outb %al, %dx
    retq

    .global in_byte
in_byte:
    mov %edi, %edx
    xor %rax, %rax
    inb %dx, %al
    retq

    .global out_word
out_word:
    mov %edi, %edx
    mov %esi, %eax
    outw %ax, %dx
    retq

    .global in_word
in_word:
    mov %edi, %edx
    xor %rax, %rax
    inw %dx, %ax
    retq

    .global out_long
out_long:
    mov %edi, %edx
    mov %esi, %eax
    outl %eax, %dx
    retq

    .global in_long
in_long:
    mov %edi, %edx
    xor %rax, %rax
    inl %dx, %eax
    retq

    .global enable_int
enable_int:
    sti
    retq

    .global disable_int
disable_int:
    cli
    retq

    .global port_read
port_read:
    retq

    .global port_write
port_write:
    retq

    .global x86_lgdt
x86_lgdt:
    lgdt (%rdi)
    retq

    .global x86_lidt
x86_lidt:
    lidt (%rdi)
    retq

    .global x86_lldt
x86_lldt:
    lldt %di
    retq

    .global x86_ltr
x86_ltr:
    ltr %di
    retq

    .global x86_load_kerncs
x86_load_kerncs:
    pushq $SELECTOR_KERNEL_CS
    leaq newcs, %rax
    push %rax
    retfq
newcs:
    retq

    .global x86_load_ds
x86_load_ds:
    mov %di, %ds
    retq

    .global x86_load_es
x86_load_es:
    mov %di, %es
    retq

    .global x86_load_fs
x86_load_fs:
    mov %di, %fs
    retq

    .global x86_load_gs
x86_load_gs:
    mov %di, %gs
    retq

    .global x86_load_ss
x86_load_ss:
    mov %di, %ss
    retq

    .global switch_k_stack
switch_k_stack:
    xor %rbp, %rbp
    mov %rdi, %rsp
    jmp *%rsi
.0:
    jmp .0

    .global fninit
fninit:
    retq

    .global fnstsw
fnstsw:
    retq

    .global fnstcw
fnstcw:
    retq

    .global fnsave
fnsave:
    retq

    .global fxsave
fxsave:
    retq

    .global frstor
frstor:
    retq

    .global fxrstor
fxrstor:
    retq

    .global frstor_end
frstor_end:
    retq

    .global fxrstor_end
fxrstor_end:
    retq

    .global frstor_fault
frstor_fault:
    retq

    .global clts
clts:
    retq

    .global ia32_read_msr
ia32_read_msr:
    mov %edi, %ecx
    mov %rdx, %r10
    rdmsr
    mov %edx, (%rsi)
    mov %eax, (%r10)
    retq

    .global ia32_write_msr
ia32_write_msr:
    mov %edi, %ecx
    mov %edx, %eax
    mov %esi, %edx
    wrmsr
    retq

    .global halt_cpu
halt_cpu:
    sti
    hlt
    retq

    .global read_cr0
read_cr0:
    mov %cr0, %rax
    retq

    .global write_cr0
write_cr0:
    mov %rdi, %cr0
    retq

    .global read_cr2
read_cr2:
    mov %cr2, %rax
    retq

    .global read_cr3
read_cr3:
    mov %cr3, %rax
    retq

    .global write_cr3
write_cr3:
    mov %rdi, %cr3
    retq

    .global read_cr4
read_cr4:
    mov %cr4, %rax
    retq

    .global write_cr4
write_cr4:
    mov %rdi, %cr4
    retq

    .global reload_cr3
reload_cr3:
    mov %cr3, %rax
    mov %rax, %cr3
    retq

    .global i8259_eoi_master
i8259_eoi_master:
    mov $0x20, %al              // EOI
    outb %al, $0x20             // INT_M_CTL
    retq

    .global i8259_eoi_slave
i8259_eoi_slave:
    mov $0x20, %al              // EOI
    outb %al, $0x20             // INT_M_CTL
    nop
    outb %al, $0xa0             // INT_S_CTL
    retq

    .global arch_pause
arch_pause:
    pause
    retq

    .global phys_copy
phys_copy:
    movq %rdi, %rax
    movq %rdx, %rcx
    shrq $3, %rcx
    andl $7, %edx
    rep movsq
    movl %edx, %ecx
    rep movsb
    xor %rax, %rax

    .global phys_copy_fault
phys_copy_fault:
    retq

    .global phys_copy_fault_in_kernel
phys_copy_fault_in_kernel:
    mov %cr2, %rax
    retq

    .global copy_user_message
copy_user_message:
    mov 0*8(%rsi), %rax
    mov %rax, 0*8(%rdi)
    mov 1*8(%rsi), %rax
    mov %rax, 1*8(%rdi)
    mov 2*8(%rsi), %rax
    mov %rax, 2*8(%rdi)
    mov 3*8(%rsi), %rax
    mov %rax, 3*8(%rdi)
    mov 4*8(%rsi), %rax
    mov %rax, 4*8(%rdi)
    mov 5*8(%rsi), %rax
    mov %rax, 5*8(%rdi)
    mov 6*8(%rsi), %rax
    mov %rax, 6*8(%rdi)
    mov 7*8(%rsi), %rax
    mov %rax, 7*8(%rdi)
    mov 8*8(%rsi), %rax
    mov %rax, 8*8(%rdi)
    mov 9*8(%rsi), %rax
    mov %rax, 9*8(%rdi)

    .global copy_user_message_end
copy_user_message_end:
    xor %rax, %rax
    retq

    .global copy_user_message_fault
copy_user_message_fault:
    mov $1, %rax
    retq

    .global memcpy
memcpy:
    movq %rdi, %rax
    movq %rdx, %rcx
    shrq $3, %rcx
    andl $7, %edx
    rep movsq
    movl %edx, %ecx
    rep movsb
    ret

    .global memset
memset:
    movq %rdi,%r9
    movq %rdx,%rcx
    andl $7,%edx
    shrq $3,%rcx
    movzbl %sil,%esi
    movabs $0x0101010101010101,%rax
    imulq %rsi,%rax
    rep stosq
    movl %edx,%ecx
    rep stosb
    movq %r9,%rax
    ret
