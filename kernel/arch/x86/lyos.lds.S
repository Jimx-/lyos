    #include <lyos/config.h>
    #include <asm/page.h>

    #ifdef CONFIG_X86_32
    #undef i386
    OUTPUT_ARCH(i386)
    #else
    OUTPUT_ARCH(i386:x86-64)
    #endif

ENTRY (_start)

    #define LOAD_PHYS_ADDR  1M

SECTIONS{
    . = KERNEL_VMA + LOAD_PHYS_ADDR;

    .text : AT(ADDR(.text) - KERNEL_VMA) {
        _text = .;
            KEEP(*(.multiboot))
            *(.head.text)
            *(.text)
        _etext = .;
    }

    . = ALIGN(4096);
    .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
        *(.rodata)
    }

    . = ALIGN(4096);
    .data : AT(ADDR(.data) - KERNEL_VMA) {
        _data = .;
            *(.data)
        _edata = .;
    }

    . = ALIGN(4096);
    .data.cpulocals : AT(ADDR(.data.cpulocals) - KERNEL_VMA) {
        _cpulocals_start = .;
            *(.data.cpulocals)
        _cpulocals_end = .;
    }

    . = ALIGN(4096);
    .bss : AT(ADDR(.bss) - KERNEL_VMA) ALIGN(4096) {
        _bss = .;
            *(COMMON)
            *(.bss)
        _ebss = .;
    }

    . = ALIGN(4096);
    .usermapped : AT(ADDR(.usermapped) - KERNEL_VMA) {
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
