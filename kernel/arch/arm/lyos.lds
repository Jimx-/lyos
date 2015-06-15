OUTPUT_ARCH(arm)
ENTRY (_lyos_start)

_PHYS_BASE = 0x80200000;
_VIR_BASE = 0xF0100000;
_KERN_OFFSET = (_VIR_BASE - _PHYS_BASE);

SECTIONS{
    . = _PHYS_BASE;

    .text : {
    	_text = .;
        	*(.text)
        _etext = .;
    }

    . = ALIGN(4096);
    .rodata : {
        *(.rodata)
    }
  
    . = ALIGN(4096);
    .data : {
    	_data = .;
        	*(.data)
        _edata = .;
    }

    . = ALIGN(4096);
    .bss : {
        _bss = .;
        	*(COMMON)
        	*(.bss)
        _ebss = .;
    }

    . = ALIGN(4096);
    .usermapped : { 
        _usermapped = .;    
            *(.usermapped)
        . = ALIGN(4096); _eusermapped = .; 
    }

    _end = .;
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
