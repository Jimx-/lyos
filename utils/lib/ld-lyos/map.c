#include <elf.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "ldso.h"
#include "env.h"
#include "debug.h"

#define MAX_SEGS 4

struct so_info* ldso_map_object(const char* pathname, int fd)
{
    struct so_info* si = ldso_alloc_info(pathname);
    int i;

    if (!si) return NULL;

    si->ehdr = mmap(NULL, pagesz, PROT_READ, MAP_SHARED, fd, 0);

    if (si->ehdr == MAP_FAILED) {
        goto failed;
    }

    if (si->ehdr->e_ident[0] != 0x7f) {
        xprintf("%s: bad elf magic number\n", pathname);
        goto failed;
    }

    if (si->ehdr->e_type != ET_EXEC && si->ehdr->e_type != ET_DYN) {
        xprintf("%s: bad elf type %x\n", pathname, si->ehdr->e_type);
        goto failed;
    }
    si->is_dynamic = (si->ehdr->e_type == ET_DYN);

    ElfW(Ehdr)* ehdr = si->ehdr;
    ElfW(Phdr)* phdr = (ElfW(Phdr)*)((char*)ehdr + ehdr->e_phoff);
    ElfW(Phdr)* phdr_tls = NULL;
    ElfW(Addr) tls_vaddr = 0;
    size_t phsize = ehdr->e_phnum * sizeof(ElfW(Phdr));
    char* phend = (char*)phdr + phsize;

    ElfW(Phdr) segs[MAX_SEGS];
    int nsegs = 0;
    for (; (char*)phdr < phend; phdr++) {
        switch (phdr->p_type) {
        case PT_LOAD:
            if (nsegs < MAX_SEGS) segs[nsegs] = *phdr;
            nsegs++;
            break;
        case PT_DYNAMIC:
            si->dynamic = (ElfW(Dyn)*)phdr->p_vaddr;
            break;
        case PT_TLS:
            phdr_tls = phdr;
            break;
        }
    }

    if (!si->dynamic) {
        xprintf("%s: not dynamically linked\n", pathname);
        goto failed;
    }

    si->entry = (char*)ehdr->e_entry;

    if (nsegs > MAX_SEGS) {
        xprintf("%s: wrong number of segments\n", pathname);
        goto failed;
    }

    off_t base_offset = rounddown(segs[0].p_offset);
    ElfW(Addr) base_vaddr = rounddown(segs[0].p_vaddr);
    ElfW(Addr) base_vlimit =
        roundup(segs[nsegs - 1].p_vaddr + segs[nsegs - 1].p_memsz);

    if (phdr_tls) {
        ++ldso_tls_dtv_generation;
        si->tls_index = ++ldso_tls_max_index;
        si->tls_size = phdr_tls->p_memsz;
        si->tls_align = phdr_tls->p_align;
        si->tls_init_size = phdr_tls->p_filesz;
        tls_vaddr = phdr_tls->p_vaddr;
    }

    if (base_offset < pagesz) {
        munmap(ehdr, pagesz);
        ehdr = MAP_FAILED;
    }

    ElfW(Addr) base_addr = (ElfW(Addr))(si->is_dynamic ? 0 : base_vaddr);
    size_t map_size = base_vlimit - base_vaddr;

    char* mapbase = mmap(
        (void*)base_addr, map_size,
        PROT_READ | PROT_WRITE | ((segs[0].p_flags & PF_X) ? PROT_EXEC : 0),
        MAP_PRIVATE | (si->is_dynamic ? 0 : MAP_FIXED), fd, base_offset);
    if (mapbase == MAP_FAILED) {
        xprintf("%s: failed to map base segment\n", pathname);
        goto failed;
    }

    dbg(("%s => %p-%p\n", pathname, mapbase, mapbase + map_size));

    for (i = 1; i < nsegs; i++) {
        ElfW(Phdr)* seg = &segs[i];
        ElfW(Addr) seg_vaddr = rounddown(seg->p_vaddr);
        off_t seg_offset = rounddown(seg->p_offset);
        ElfW(Addr) seg_flimit = roundup(seg->p_vaddr + seg->p_filesz);
        ElfW(Addr) seg_mlimit = roundup(seg->p_vaddr + seg->p_memsz);
        ElfW(Addr) clear_vaddr = seg->p_vaddr + seg->p_filesz;
        void* map_addr = mapbase + (seg_vaddr - base_vaddr);
        int map_prot =
            PROT_READ | PROT_WRITE | ((seg->p_flags & PF_X) ? PROT_EXEC : 0);

        if (mmap(map_addr, seg_flimit - seg_vaddr, map_prot,
                 MAP_PRIVATE | MAP_FIXED, fd, seg_offset) == MAP_FAILED) {
            xprintf("%s: failed to map segment\n", pathname);
            goto failed;
        }

        if (seg_mlimit > seg_flimit) {
            if (mmap(mapbase + (seg_flimit - base_vaddr),
                     seg_mlimit - seg_flimit, map_prot,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1,
                     0) == MAP_FAILED) {
                xprintf("%s: failed to map segment\n", pathname);
                goto failed;
            }
        }

        if (seg->p_memsz > seg->p_filesz && seg_flimit > clear_vaddr) {
            void* clear_addr = mapbase + (clear_vaddr - base_vaddr);
            size_t clear_size = seg_flimit - clear_vaddr;
            memset(clear_addr, 0, clear_size);
        }
    }

    si->mapbase = mapbase;
    si->mapsize = map_size;
    si->relocbase = mapbase - base_vaddr;

    if (si->dynamic)
        si->dynamic = (ElfW(Dyn)*)(si->relocbase + (ElfW(Addr))si->dynamic);
    if (si->entry) si->entry = (char*)(si->relocbase + (ElfW(Addr))si->entry);

    if (phdr_tls) {
        si->tls_init = mapbase + tls_vaddr;
    }

    return si;

failed:
    if (ehdr != MAP_FAILED) munmap(ehdr, pagesz);
    si->flags = 0;
    return NULL;
}
