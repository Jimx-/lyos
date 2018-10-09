#ifndef _LIBEXEC_EXEC_ELF_H_
#define _LIBEXEC_EXEC_ELF_H_

#include <lyos/config.h>
#include <elf.h>

#ifdef __LP64__
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
#else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
#endif

#endif // _LIBEXEC_EXEC_ELF_H_
