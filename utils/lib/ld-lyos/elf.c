#include <stdio.h>
#include <elf.h>

#include "ldso.h"

#define MAX_NEEDED_ENTRIES 128
static struct needed_entry needed_entries[MAX_NEEDED_ENTRIES];
static int n_ent_count = 0;

static struct needed_entry* alloc_needed_entry()
{
    if (n_ent_count >= MAX_NEEDED_ENTRIES) ldso_die("too many needed objects");
    return &needed_entries[n_ent_count++];
}

int ldso_process_dynamic(struct so_info* si)
{
    ElfW(Dyn) * dp;
    int use_pltrel = 0, use_pltrela = 0;
    ElfW(Addr) pltrel, pltrelsz;
    ElfW(Addr) relsz, relasz;
    ElfW(Addr) init = 0, fini = 0;
    struct needed_entry* needed;
    struct needed_entry** needed_tail = &si->needed;

    si->needed = NULL;

    for (dp = si->dynamic; dp->d_tag != DT_NULL; dp++) {
        switch (dp->d_tag) {
        case DT_REL:
            si->rel = (ElfW(Rel)*)(si->relocbase + dp->d_un.d_ptr);
            break;
        case DT_RELSZ:
            relsz = dp->d_un.d_val;
            break;
        case DT_RELA:
            si->rela = (ElfW(Rela)*)(si->relocbase + dp->d_un.d_ptr);
            break;
        case DT_RELASZ:
            relasz = dp->d_un.d_val;
            break;
        case DT_PLTREL:
            use_pltrela = dp->d_un.d_val == DT_RELA;
            use_pltrel = dp->d_un.d_val == DT_REL;
            break;
        case DT_JMPREL:
            pltrel = dp->d_un.d_ptr;
            break;
        case DT_PLTRELSZ:
            pltrelsz = dp->d_un.d_val;
            break;
        case DT_SYMTAB:
            si->symtab = (ElfW(Sym)*)(si->relocbase + dp->d_un.d_ptr);
            break;
        case DT_STRTAB:
            si->strtab = (char*)(si->relocbase + dp->d_un.d_ptr);
            break;
        case DT_STRSZ:
            si->strtabsz = dp->d_un.d_val;
            break;
        case DT_PLTGOT:
            si->pltgot = (ElfW(Addr)*)(si->relocbase + dp->d_un.d_ptr);
            break;
        case DT_INIT:
            init = dp->d_un.d_ptr;
            break;
        case DT_INIT_ARRAY:
            si->init_array = (ElfW(Addr)*)(si->relocbase + dp->d_un.d_ptr);
            break;
        case DT_INIT_ARRAYSZ:
            si->init_array_size = dp->d_un.d_val / sizeof(ElfW(Addr));
            break;
        case DT_FINI:
            fini = dp->d_un.d_ptr;
            break;
        case DT_FINI_ARRAY:
            si->fini_array = (ElfW(Addr)*)(si->relocbase + dp->d_un.d_ptr);
            break;
        case DT_FINI_ARRAYSZ:
            si->fini_array_size = dp->d_un.d_val / sizeof(ElfW(Addr));
            break;
        case DT_NEEDED:
            needed = alloc_needed_entry();
            needed->name = dp->d_un.d_val;
            needed->si = NULL;
            needed->next = NULL;

            *needed_tail = needed;
            needed_tail = &needed->next;
            break;
        case DT_HASH: {
            int* hash_table = (int*)(si->relocbase + dp->d_un.d_ptr);

            si->nbuckets = hash_table[0];
            si->nchains = hash_table[1];
            si->buckets = hash_table + 2;
            si->chains = si->buckets + si->nbuckets;
            break;
        }
        }
    }

    si->relend = (ElfW(Rel)*)((char*)si->rel + relsz);
    si->relaend = (ElfW(Rela)*)((char*)si->rela + relasz);
    if (use_pltrel) {
        si->pltrel = (ElfW(Rel)*)(si->relocbase + pltrel);
        si->pltrelend = (ElfW(Rel)*)(si->relocbase + pltrel + pltrelsz);
    }
    if (use_pltrela) {
        si->pltrela = (ElfW(Rela)*)(si->relocbase + pltrel);
        si->pltrelaend = (ElfW(Rela)*)(si->relocbase + pltrel + pltrelsz);
    }

    if (init) {
        si->init = (void (*)(void))(si->relocbase + init);
    }
    if (fini) {
        si->fini = (void (*)(void))(si->relocbase + fini);
    }
}

int ldso_process_phdr(struct so_info* si, ElfW(Phdr) * phdr, int phnum)
{
    ElfW(Phdr) * hdr;
    ElfW(Addr) vaddr;
    int nsegs = 0;

    si->phnum = phnum;

    for (hdr = phdr; hdr < phdr + phnum; hdr++) {
        if (hdr->p_type == PT_PHDR) {
            si->phdr = (ElfW(Phdr)*)hdr->p_vaddr;
            si->relocbase = (char*)((char*)phdr - (char*)si->phdr);
            break;
        }
    }

    for (hdr = phdr; hdr < phdr + phnum; hdr++) {
        vaddr = (ElfW(Addr))(si->relocbase + hdr->p_vaddr);
        switch (hdr->p_type) {
        case PT_LOAD:
            if (nsegs == 0) {
                si->mapbase = (void*)vaddr;
            } else {
                si->mapsize =
                    roundup(vaddr + hdr->p_memsz) - (uintptr_t)si->mapsize;
            }
            nsegs++;
            break;
        case PT_DYNAMIC:
            si->dynamic = (ElfW(Dyn)*)vaddr;
            break;
        case PT_TLS:
            si->tls_index = 1;
            si->tls_size = hdr->p_memsz;
            si->tls_align = hdr->p_align;
            si->tls_init_size = hdr->p_filesz;
            si->tls_init = (void*)(uintptr_t)hdr->p_vaddr;
            break;
        }
    }

    return 0;
}
