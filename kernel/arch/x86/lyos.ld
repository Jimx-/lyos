OUTPUT_ARCH(i386)
ENTRY (_start)
SECTIONS{
    . = 0xF0100000;

    .text : AT(ADDR(.text) - 0xF0000000) {
    	_text = .;
        	*(.text)
        _etext = .;
    }

    . = ALIGN(4096);
    .rodata : AT(ADDR(.rodata) - 0xF0000000) {
        *(.rodata)
    }
  
    . = ALIGN(4096);
    .data : AT(ADDR(.data) - 0xF0000000) {
    	_data = .;
        	*(.data)
        _edata = .;
    }

    . = ALIGN(4096);
    .bss : AT(ADDR(.bss) - 0xF0000000) {
        _bss = .;
        	*(COMMON)
        	*(.bss)
        _ebss = .;
    }

    . = ALIGN(4096);
    .usermapped : AT(ADDR(.usermapped) - 0xF0000000) { 
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