    .section .text

    .global out_byte
out_byte:
    retq

    .global in_byte
in_byte:
    retq

    .global out_word
out_word:
    retq

    .global in_word
in_word:
    retq

    .global out_long
out_long:
    retq

    .global in_long
in_long:
    retq

    .global enable_int
enable_int:
    retq

    .global disable_int
disable_int:
    retq

    .global port_read
port_read:
    retq

    .global port_write
port_write:
    retq

    .global x86_lgdt
x86_lgdt:
    retq

    .global x86_lidt
x86_lidt:
    retq

    .global x86_lldt
x86_lldt:
    retq

    .global x86_ltr
x86_ltr:
    retq

    .global x86_load_kerncs
x86_load_kerncs:
    retq

    .global x86_load_ds
x86_load_ds:
    retq

    .global x86_load_es
x86_load_es:
    retq

    .global x86_load_fs
x86_load_fs:
    retq

    .global x86_load_gs
x86_load_gs:
    retq

    .global x86_load_ss
x86_load_ss:
    retq

    .global switch_k_stack
switch_k_stack:
    retq

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
    retq

    .global ia32_write_msr
ia32_write_msr:
    retq

    .global halt_cpu
halt_cpu:
    retq

    .global read_cr0
read_cr0:
    retq

    .global write_cr0
write_cr0:
    retq

    .global read_cr2
read_cr2:
    retq

    .global read_cr3
read_cr3:
    retq

    .global write_cr3
write_cr3:
    retq

    .global read_cr4
read_cr4:
    retq

    .global write_cr4
write_cr4:
    retq

    .global reload_cr3
reload_cr3:
    retq

    .global i8259_eoi_master
i8259_eoi_master:
    retq

    .global i8259_eoi_slave
i8259_eoi_slave:
    retq

    .global arch_pause
arch_pause:
    retq

    .global phys_copy
phys_copy:
    retq

    .global phys_copy_fault
phys_copy_fault:
    retq

    .global phys_copy_fault_in_kernel
phys_copy_fault_in_kernel:
    retq

    .global copy_user_message
copy_user_message:
    retq

    .global copy_user_message_end
copy_user_message_end:
    retq

    .global copy_user_message_fault
copy_user_message_fault:
    retq
