OUTPUT_ARCH(arm)
ENTRY (_lyos_start)

_PHYS_BASE = 0x80200000;
_VIR_BASE = 0xF0100000;
_KERN_OFFSET = (_VIR_BASE - _PHYS_BASE);

SECTIONS{
    . = _PHYS_BASE;

    . = ALIGN(4096);
    .unpaged_text : {
        _unpaged_text = .;
            *(.head)
            *(.unpaged_text)
        _unpaged_etext = .;
    }
  
    . = ALIGN(4096);
    .unpaged_data : {
        _unpaged_data = .;
            *(.unpaged_data)
        _unpaged_edata = .;
    }

    . = ALIGN(4096);
    .unpaged_bss : {
        _unpaged_bss = .;
            *(.unpaged_bss)
        _unpaged_ebss = .;
    }

    . += _KERN_OFFSET;

    .text :  AT(ADDR(.text) - _KERN_OFFSET){
    	_text = .;
        	*(.text)
        _etext = .;
    }

    . = ALIGN(4096);
    .rodata : AT(ADDR(.rodata) - _KERN_OFFSET) {
        *(.rodata)
    }
  
    . = ALIGN(4096);
    .data : AT(ADDR(.data) - _KERN_OFFSET) {
    	_data = .;
        	*(.data)
        _edata = .;
    }

    . = ALIGN(4096);
    .bss : AT(ADDR(.bss) - _KERN_OFFSET) {
        _bss = .;
        	*(COMMON)
        	*(.bss)
        _ebss = .;
    }

    . = ALIGN(4096);
    .usermapped : AT(ADDR(.usermapped) - _KERN_OFFSET) { 
        _usermapped = .;    
            *(.usermapped)
        . = ALIGN(4096); _eusermapped = .; 
    }

    . = ALIGN(4096);
    .arch.info.init : AT(ADDR(.arch.info.init) - _KERN_OFFSET) {
        _arch_init_start = .;
            *(.arch.info.init)
        _arch_init_end = .;
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
