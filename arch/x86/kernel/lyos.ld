OUTPUT_ARCH(i386)
ENTRY (_start)
SECTIONS{
    . = 0xF0100000;
    .text : AT(ADDR(.text) - 0xF0000000) {
    	_text = .;
        	*(.text)
        _etext = .;
    }

    .rodata : AT(ADDR(.rodata) - 0xF0000000) {
        *(.rodata)
    }
  
    .data : AT(ADDR(.data) - 0xF0000000) {
    	_data = .;
        	*(.data)
        _edata = .;
    }
    .bss : AT(ADDR(.bss) - 0xF0000000) {
        _bss = .;
        	*(COMMON)
        	*(.bss)
        _ebss = .;
    }
    _end = .;
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}