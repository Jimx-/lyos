file /home/jimx/projects/lyos/trunk/arch/x86/lyos.bin
target remote 127.0.0.1:1234 
set disassembly-flavor intel
set archi i386
b calibrate_handler
c
