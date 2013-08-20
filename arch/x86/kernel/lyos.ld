OUTPUT_ARCH(i386)
ENTRY (_start)
SECTIONS{
    . = 0x00100000;
    .text :{
    	_text = .;
        	*(.text)
        _etext = .;
    }

    .rodata : {
        *(.rodata)
    }
  
    .data : {
    	_data = .;
        	*(.data)
        _edata = .;
    }
    .bss : {
        _bss = .;
        	*(COMMON)
        	*(.bss)
        _ebss = .;
    }
    _end = .;
}