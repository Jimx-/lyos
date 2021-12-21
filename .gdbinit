file /home/jimx/projects/lyos/arch/x86/lyos.elf.64
target remote 127.0.0.1:1234
# set disassembly-flavor intel
# set archi i386
hbreak finish_bsp_booting
layout src
