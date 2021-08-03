#ifndef _LIBEXEC_EXEC_ELF_H_
#define _LIBEXEC_EXEC_ELF_H_

#include <lyos/config.h>
#include <elf.h>

#ifdef __LP64__
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
typedef Elf64_auxv_t Elf_auxv_t;
#else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
typedef Elf32_auxv_t Elf_auxv_t;
#endif

#endif // _LIBEXEC_EXEC_ELF_H_
