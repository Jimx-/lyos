#include <asm/page.h>

OUTPUT_ARCH(aarch64)
ENTRY(_text)

_VIR_BASE = KERNEL_VMA;

SECTIONS
{
    . = _VIR_BASE;

    .head.text : {
        _text = .;
        *(.head.text)
    }

    .text : ALIGN(0x10000) {
          _stext = .;
          *(.text)

          . = ALIGN(ARCH_PG_SIZE);
          __idmap_text_start = .;
          *(.idmap.text)
          __idmap_text_end = .;
    }

    . = ALIGN(0x10000);
    _etext = .;

    . = ALIGN(ARCH_PG_SIZE);
    .rodata : {
        *(.rodata)
    }

    . = ALIGN(0x10000);
    .data : {
        _data = .;
            *(.data)
        _edata = .;
    }

    . = ALIGN(ARCH_PG_SIZE);
    .data.cpulocals : {
        _cpulocals_start = .;
            *(.data.cpulocals)
        _cpulocals_end = .;
    }

    . = ALIGN(ARCH_PG_SIZE);
    .bss : {
        _bss = .;
            *(COMMON)
            *(.bss)
        . = ALIGN(ARCH_PG_SIZE);
        _ebss = .;
    }

    . = ALIGN(ARCH_PG_SIZE);
    idmap_pg_dir = .;
    . += IDMAP_DIR_SIZE;
    init_pg_dir = .;
    . += INIT_DIR_SIZE;

   _KERN_SIZE = . - _VIR_BASE;

    _end = .;
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
