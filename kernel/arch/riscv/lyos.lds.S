#include <asm/page.h>

OUTPUT_ARCH(riscv)
ENTRY (_lyos_start)

_VIR_BASE = KERNEL_VMA;

SECTIONS{
    . = _VIR_BASE;
    _lyos_start = .;

    . = ALIGN(4096);
    .unpaged_text : AT(ADDR(.unpaged_text) - _VIR_BASE) {
        _unpaged_text = .;
            *(.head)
            *(.unpaged_text)
        _unpaged_etext = .;
    }

    . = ALIGN(4096);
    .unpaged_data : AT(ADDR(.unpaged_data) - _VIR_BASE) {
        _unpaged_data = .;
            *(.unpaged_data)
        _unpaged_edata = .;
    }

    . = ALIGN(4096);
    .unpaged_bss : AT(ADDR(.unpaged_bss) - _VIR_BASE) {
        _unpaged_bss = .;
            *(.unpaged_bss)
        _unpaged_ebss = .;
    }

    . = ALIGN(4096);
    .text : AT(ADDR(.text) - _VIR_BASE) {
    	_text = .;
        	*(.text)
        _etext = .;
    }

    . = ALIGN(4096);
    .rodata : AT(ADDR(.rodata) - _VIR_BASE) {
        *(.rodata)
    }
  
    . = ALIGN(4096);
    .data : AT(ADDR(.data) - _VIR_BASE) {
        __global_pointer$ = . + 0x800;
    	_data = .;
        	*(.data)
        _edata = .;
    }

    . = ALIGN(4096);
    .bss : AT(ADDR(.bss) - _VIR_BASE) {
        _bss = .;
        	*(COMMON)
        	*(.bss)
        _ebss = .;
    }

    . = ALIGN(4096);
    .usermapped : AT(ADDR(.usermapped) - _VIR_BASE) {
        _usermapped = .;    
            *(.usermapped)
        . = ALIGN(4096); _eusermapped = .; 
    }

    _KERN_SIZE = . - _VIR_BASE;

    _end = .;
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
