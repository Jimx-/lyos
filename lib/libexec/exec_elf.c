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

#include "lyos/type.h"
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
#include <elf.h>
#include "libexec.h"
#include "page.h"
#include "lyos/vm.h"
#include <sys/mman.h>

//#define ELF_DEBUG

#define roundup(x, a)  do {\
                        if ((x) % (a) != 0) {   \
                            (x) = (x) + (a) - ((x) % (a));    \
                        }   \
                    } while(0)

PRIVATE int elf_check_header(Elf32_Ehdr * elf_hdr);
PRIVATE int elf_unpack(char * hdr, Elf32_Ehdr ** elf_hdr, Elf32_Phdr ** prog_hdr);

PRIVATE int elf_check_header(Elf32_Ehdr * elf_hdr)
{
	if (elf_hdr->e_ident[0] != 0x7f || (elf_hdr->e_type != ET_EXEC && elf_hdr->e_type != ET_DYN)) {
		return ENOEXEC;
	}

	return 0;
}

PRIVATE int elf_unpack(char * hdr, Elf32_Ehdr ** elf_hdr, Elf32_Phdr ** prog_hdr)
{
	*elf_hdr = (Elf32_Ehdr *)hdr;
	*prog_hdr = (Elf32_Phdr *)(hdr + (*elf_hdr)->e_phoff);

	return 0;
}

PUBLIC int elf_is_dynamic(char* hdr, size_t hdr_len, char* interp, size_t maxlen)
{
    Elf32_Ehdr* elf_hdr;
    Elf32_Phdr* prog_hdr;

    if (elf_unpack(hdr, &elf_hdr, &prog_hdr)) return 0;

    int i;
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        switch (prog_hdr[i].p_type) {
        case PT_INTERP:
            if(!interp) return 1;
            if(prog_hdr[i].p_filesz >= maxlen)
                return -1;
            if(prog_hdr[i].p_offset + prog_hdr[i].p_filesz >= hdr_len)
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

PUBLIC int libexec_load_elf(struct exec_info * execi)
{
    int retval;
    Elf32_Ehdr * elf_hdr;
    Elf32_Phdr * prog_hdr;
    u32 load_base = 0xffffffff;

    if ((retval = elf_check_header((Elf32_Ehdr *)execi->header)) != 0) return retval;

    if ((retval = elf_unpack(execi->header, &elf_hdr, &prog_hdr)) != 0) return retval;

    if (execi->clearproc) execi->clearproc(execi);

    execi->phnum = elf_hdr->e_phnum;

    int i;
    /* load every segment */
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        Elf32_Phdr * phdr = &prog_hdr[i];
        off_t foffset;
        u32 vaddr;
        size_t fsize, memsize, clearend = 0;
        int mmap_prot = PROT_READ;

        if (phdr->p_flags & PF_W) mmap_prot |= PROT_WRITE;

        if (phdr->p_type == PT_PHDR) execi->phdr = phdr->p_vaddr;

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) continue;    /* ignore */

        if ((u32)phdr->p_vaddr < load_base) load_base = (u32)phdr->p_vaddr;

        if((phdr->p_vaddr % PG_SIZE) != (phdr->p_offset % PG_SIZE)) {
            printl("libexec: unaligned ELF program?\n");
        }

        foffset = phdr->p_offset;
        fsize = phdr->p_filesz;
        vaddr = phdr->p_vaddr;
        memsize = phdr->p_memsz;

        /* clear memory beyond file size */
        size_t rem = (vaddr + fsize) % PG_SIZE;
        if (rem && (fsize < memsize)) {
            clearend = PG_SIZE - rem;
        }

#ifdef ELF_DEBUG
        printl("libexec: segment %d: vaddr: 0x%x, size: { file: 0x%x, mem: 0x%x }, foffset: 0x%x\n", i, vaddr, fsize, memsize, foffset);
#endif

        /* align */
        u32 alignment = vaddr % PG_SIZE;
        foffset -= alignment;
        vaddr -= alignment;
        fsize += alignment;
        memsize += alignment;

        roundup(memsize, PG_SIZE);
        roundup(fsize, PG_SIZE);
        if ((phdr->p_flags & PF_X) != 0)
            execi->text_size = memsize;
        else {
            execi->data_size = memsize;
            execi->brk = phdr->p_vaddr + phdr->p_memsz + 1;
            roundup(execi->brk, sizeof(int));
        }

        //if (0) {
        if (execi->memmap && execi->memmap(execi, vaddr, fsize, foffset, mmap_prot, clearend) == 0) {
            /* allocate remaining memory */
            if (memsize > fsize) {
                size_t rem_size = memsize - fsize;
                vir_bytes rem_start = vaddr + fsize;

                if (execi->allocmem(execi, rem_start, rem_size) != 0) {
                    if (execi->clearproc) execi->clearproc(execi);
                    return ENOMEM;
                }

                execi->clearmem(execi, rem_start, rem_size);
            }
        } else {
            if (execi->allocmem(execi, vaddr, memsize) != 0) {
                if (execi->clearproc) execi->clearproc(execi);
                return ENOMEM;
            }
            
            if (execi->copymem(execi, foffset, vaddr, fsize) != 0) {
                if (execi->clearproc) execi->clearproc(execi);
                return ENOMEM;
            }

            /* clear remaining memory */
            int zero_len = phdr->p_vaddr - vaddr;
            if (zero_len) {
#ifdef ELF_DEBUG
                printl("libexec: clear memory 0x%x - 0x%x\n", vaddr, vaddr + zero_len);
#endif
                execi->clearmem(execi, vaddr, zero_len);
            }

            u32 fileend = phdr->p_vaddr + phdr->p_filesz;
            u32 memend = vaddr + memsize;
            zero_len = memend - fileend;
            if (zero_len) {
#ifdef ELF_DEBUG
                printl("libexec: clear memory 0x%x - 0x%x\n", fileend, fileend + zero_len);
#endif
                execi->clearmem(execi, fileend, zero_len);
            }
        }
    }

    /* allocate stack */
    if (execi->allocmem_prealloc(execi, execi->stack_top - execi->stack_size, execi->stack_size) != 0) {
        if (execi->clearproc) execi->clearproc(execi);
        return ENOMEM;
    }
    //execi->clearmem(execi, execi->stack_top - execi->stack_size, execi->stack_size);
    execi->entry_point = elf_hdr->e_entry;
    execi->load_base = (void*)load_base;

    return 0;
}

#if 1
PUBLIC int libexec_load_elf_dbg(struct exec_info * execi)
{
    int retval;
    Elf32_Ehdr * elf_hdr;
    Elf32_Phdr * prog_hdr;
    u32 load_base = 0xffffffff;

    if ((retval = elf_check_header((Elf32_Ehdr *)execi->header)) != 0) return retval;

    if ((retval = elf_unpack(execi->header, &elf_hdr, &prog_hdr)) != 0) return retval;

    if (execi->clearproc) execi->clearproc(execi);

    execi->phnum = elf_hdr->e_phnum;

    int i;
    /* load every segment */
    for (i = 0; i < elf_hdr->e_phnum; i++) {
        Elf32_Phdr * phdr = &prog_hdr[i];
        off_t foffset;
        u32 vaddr;
        size_t fsize, memsize, clearend = 0;
        int mmap_prot = PROT_READ;

        if (phdr->p_flags & PF_W) mmap_prot |= PROT_WRITE;

        if (phdr->p_type == PT_PHDR) execi->phdr = phdr->p_vaddr;

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) continue;    /* ignore */

        if ((u32)phdr->p_vaddr < load_base) load_base = (u32)phdr->p_vaddr;

        if((phdr->p_vaddr % PG_SIZE) != (phdr->p_offset % PG_SIZE)) {
            printl("libexec: unaligned ELF program?\n");
        }

        foffset = phdr->p_offset;
        fsize = phdr->p_filesz;
        vaddr = phdr->p_vaddr;
        memsize = phdr->p_memsz;

        /* clear memory beyond file size */
        size_t rem = (vaddr + fsize) % PG_SIZE;
        if (rem && (fsize < memsize)) {
            clearend = PG_SIZE - rem;
        }

        printl("libexec: segment %d: vaddr: 0x%x, size: { file: 0x%x, mem: 0x%x }, foffset: 0x%x\n", i, vaddr, fsize, memsize, foffset);

        /* align */
        u32 alignment = vaddr % PG_SIZE;
        foffset -= alignment;
        vaddr -= alignment;
        fsize += alignment;
        memsize += alignment;

        roundup(memsize, PG_SIZE);
        roundup(fsize, PG_SIZE);
        if ((phdr->p_flags & PF_X) != 0)
            execi->text_size = memsize;
        else {
            execi->data_size = memsize;
            execi->brk = phdr->p_vaddr + phdr->p_memsz + 1;
            roundup(execi->brk, sizeof(int));
        }

        //if (0) {
        if (execi->memmap && execi->memmap(execi, vaddr, fsize, foffset, mmap_prot, clearend) == 0) {
            printl("libexec: memmap 0x%x - 0x%x\n", vaddr, vaddr + fsize);
            /* allocate remaining memory */
            if (memsize > fsize) {
                size_t rem_size = memsize - fsize;
                vir_bytes rem_start = vaddr + fsize;

                if (execi->allocmem(execi, rem_start, rem_size) != 0) {
                    if (execi->clearproc) execi->clearproc(execi);
                    return ENOMEM;
                }

                execi->clearmem(execi, rem_start, rem_size);
            }
        } else {
            if (execi->allocmem(execi, vaddr, memsize) != 0) {
                if (execi->clearproc) execi->clearproc(execi);
                return ENOMEM;
            }
            
            if (execi->copymem(execi, foffset, vaddr, fsize) != 0) {
                if (execi->clearproc) execi->clearproc(execi);
                return ENOMEM;
            }

            /* clear remaining memory */
            int zero_len = phdr->p_vaddr - vaddr;
            if (zero_len) {
                printl("libexec: clear memory 0x%x - 0x%x\n", vaddr, vaddr + zero_len);
                execi->clearmem(execi, vaddr, zero_len);
            }

            u32 fileend = phdr->p_vaddr + phdr->p_filesz;
            u32 memend = vaddr + memsize;
            zero_len = memend - fileend;
            if (zero_len) {
                printl("libexec: clear memory 0x%x - 0x%x\n", fileend, fileend + zero_len);
                execi->clearmem(execi, fileend, zero_len);
            }
        }
    }

    /* allocate stack */
    if (execi->allocmem_prealloc(execi, execi->stack_top - execi->stack_size, execi->stack_size) != 0) {
        if (execi->clearproc) execi->clearproc(execi);
        return ENOMEM;
    }
    //execi->clearmem(execi, execi->stack_top - execi->stack_size, execi->stack_size);
    execi->entry_point = elf_hdr->e_entry;
    execi->load_base = (void*)load_base;

    return 0;
}
#endif
