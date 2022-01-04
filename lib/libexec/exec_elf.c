/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "libexec.h"
#include <asm/page.h>
#include <lyos/vm.h>
#include <sys/mman.h>

#include "exec_elf.h"

// #define ELF_DEBUG

static int elf_check_header(Elf_Ehdr* elf_hdr);
static int elf_unpack(char* hdr, Elf_Ehdr** elf_hdr, Elf_Phdr** prog_hdr);

static int elf_check_header(Elf_Ehdr* elf_hdr)
{
    if (elf_hdr->e_ident[0] != 0x7f ||
        (elf_hdr->e_type != ET_EXEC && elf_hdr->e_type != ET_DYN)) {
        return ENOEXEC;
    }

    return 0;
}

static int elf_unpack(char* hdr, Elf_Ehdr** elf_hdr, Elf_Phdr** prog_hdr)
{
    *elf_hdr = (Elf_Ehdr*)hdr;
    *prog_hdr = (Elf_Phdr*)(hdr + (*elf_hdr)->e_phoff);

    return 0;
}

int elf_is_dynamic(char* hdr, size_t hdr_len, char* interp, size_t maxlen)
{
    Elf_Ehdr* elf_hdr;
    Elf_Phdr* prog_hdr;

    if (elf_check_header((Elf_Ehdr*)hdr)) return 0;

    if (elf_unpack(hdr, &elf_hdr, &prog_hdr)) return 0;

    int i;
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        switch (prog_hdr[i].p_type) {
        case PT_INTERP:
            if (!interp) return 1;
            if (prog_hdr[i].p_filesz >= maxlen) return -1;
            if (prog_hdr[i].p_offset + prog_hdr[i].p_filesz >= hdr_len)
                return -1;
            memcpy(interp, hdr + prog_hdr[i].p_offset, prog_hdr[i].p_filesz);
            interp[prog_hdr[i].p_filesz] = '\0';
            return 1;
        default:
            continue;
        }
    }

    return 0;
}

int libexec_load_elf(struct exec_info* execi)
{
    int retval;
    Elf_Ehdr* elf_hdr;
    Elf_Phdr* prog_hdr;
    uintptr_t load_base = 0;
    int first = TRUE;

    if ((retval = elf_check_header((Elf_Ehdr*)execi->header)) != 0)
        return retval;

    if ((retval = elf_unpack(execi->header, &elf_hdr, &prog_hdr)) != 0)
        return retval;

    if (execi->clearproc) execi->clearproc(execi);

    execi->phnum = elf_hdr->e_phnum;

    int i;
    /* load every segment */
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        Elf_Phdr* phdr = &prog_hdr[i];
        off_t foffset;
        uintptr_t p_vaddr, vaddr;
        size_t fsize, memsize, clearend = 0;
        int mmap_prot = 0;

        if (phdr->p_flags & PF_R) mmap_prot |= PROT_READ;
        if (phdr->p_flags & PF_W) mmap_prot |= PROT_WRITE;
        if (phdr->p_flags & PF_X) mmap_prot |= PROT_EXEC;

        if (phdr->p_type == PT_PHDR) execi->phdr = (void*)phdr->p_vaddr;

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0)
            continue; /* ignore */

        if ((phdr->p_vaddr % ARCH_PG_SIZE) != (phdr->p_offset % ARCH_PG_SIZE)) {
            printl("libexec: unaligned ELF program?\n");
        }

        foffset = phdr->p_offset;
        fsize = phdr->p_filesz;
        p_vaddr = vaddr = phdr->p_vaddr + execi->load_offset;
        memsize = phdr->p_memsz;

        /* clear memory beyond file size */
        size_t rem = (vaddr + fsize) % ARCH_PG_SIZE;
        if (rem && (fsize < memsize)) {
            clearend = ARCH_PG_SIZE - rem;
        }

#ifdef ELF_DEBUG
        printl("libexec: segment %d: vaddr: 0x%x, size: { file: 0x%x, mem: "
               "0x%x }, foffset: 0x%x\n",
               i, vaddr, fsize, memsize, foffset);
#endif

        /* align */
        size_t alignment = vaddr % ARCH_PG_SIZE;
        foffset -= alignment;
        vaddr -= alignment;
        fsize += alignment;
        memsize += alignment;

        memsize = roundup(memsize, ARCH_PG_SIZE);
        fsize = roundup(fsize, ARCH_PG_SIZE);

        if (first || load_base > vaddr) load_base = vaddr;
        first = FALSE;

        if ((phdr->p_flags & PF_X) != 0)
            execi->text_size = memsize;
        else {
            execi->data_size = memsize;
        }

        if (execi->memmap && execi->memmap(execi, (void*)vaddr, fsize, foffset,
                                           mmap_prot, clearend) == 0) {
            /* allocate remaining memory */
            if (memsize > fsize) {
                size_t rem_size = memsize - fsize;
                void* rem_start = (void*)(vaddr + fsize);

                if (execi->allocmem(execi, rem_start, rem_size, mmap_prot) !=
                    0) {
                    if (execi->clearproc) execi->clearproc(execi);
                    return ENOMEM;
                }

                execi->clearmem(execi, rem_start, rem_size);
            }
        } else {
            if (execi->allocmem(execi, (void*)vaddr, memsize, mmap_prot) != 0) {
                if (execi->clearproc) execi->clearproc(execi);
                return ENOMEM;
            }

            if (execi->copymem(execi, foffset, (void*)vaddr, fsize) != 0) {
                if (execi->clearproc) execi->clearproc(execi);
                return ENOMEM;
            }

            /* clear remaining memory */
            size_t zero_len = p_vaddr - vaddr;
            if (zero_len) {
#ifdef ELF_DEBUG
                printl("libexec: clear memory 0x%x - 0x%x\n", vaddr,
                       vaddr + zero_len);
#endif
                execi->clearmem(execi, (void*)vaddr, zero_len);
            }

            size_t fileend = p_vaddr + phdr->p_filesz;
            size_t memend = vaddr + memsize;
            zero_len = memend - fileend;
            if (zero_len) {
#ifdef ELF_DEBUG
                printl("libexec: clear memory 0x%x - 0x%x\n", fileend,
                       fileend + zero_len);
#endif
                execi->clearmem(execi, (void*)fileend, zero_len);
            }
        }
    }

    /* allocate stack */
    if (execi->allocmem_prealloc &&
        execi->allocmem_prealloc(execi, execi->stack_top - execi->stack_size,
                                 execi->stack_size,
                                 PROT_READ | PROT_WRITE) != 0) {
        if (execi->clearproc) execi->clearproc(execi);
        return ENOMEM;
    }
    // execi->clearmem(execi, execi->stack_top - execi->stack_size,
    // execi->stack_size);
    execi->entry_point = (void*)elf_hdr->e_entry + execi->load_offset;
    execi->load_base = (void*)load_base;

    return 0;
}
