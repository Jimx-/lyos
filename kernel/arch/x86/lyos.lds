    OUTPUT_ARCH(i386:x86-64)
ENTRY (_start)
SECTIONS{
    . = 0xffffffff80000000 + 1M;
    .text : AT(ADDR(.text) - 0xffffffff80000000) {
        _text = .;
            KEEP(*(.multiboot))
            *(.head.text)
            *(.text)
        _etext = .;
    }
    . = ALIGN(4096);
    .rodata : AT(ADDR(.rodata) - 0xffffffff80000000) {
        *(.rodata)
    }
    . = ALIGN(4096);
    .data : AT(ADDR(.data) - 0xffffffff80000000) {
        _data = .;
            *(.data)
        _edata = .;
    }
    . = ALIGN(4096);
    .data.cpulocals : AT(ADDR(.data.cpulocals) - 0xffffffff80000000) {
        _cpulocals_start = .;
            *(.data.cpulocals)
        _cpulocals_end = .;
    }
    . = ALIGN(4096);
    .bss : AT(ADDR(.bss) - 0xffffffff80000000) {
        _bss = .;
            *(COMMON)
            *(.bss)
        _ebss = .;
    }
    . = ALIGN(4096);
    .usermapped : AT(ADDR(.usermapped) - 0xffffffff80000000) {
        _usermapped = .;
            *(.usermapped)
        . = ALIGN(4096);
        _eusermapped = .;
    }
    _end = .;
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
