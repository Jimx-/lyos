#include <elf.h>
#include <string.h>

#include "ldso.h"

static int ldso_do_copy_relocation(struct so_info* si, const ElfW(Rela) * rela)
{
    void* dest_addr = (void*)(si->relocbase + rela->r_offset);
    ElfW(Sym)* dest_sym = si->symtab + ELFW(R_SYM)(rela->r_info);
    size_t size = dest_sym->st_size;
    char* name = si->strtab + dest_sym->st_name;
    unsigned long hash = ldso_elf_hash(name);

    struct so_info* src_obj;
    ElfW(Sym) * src_sym;
    for (src_obj = si->next; src_obj != NULL; src_obj = src_obj->next) {
        src_sym = ldso_lookup_symbol_obj(name, hash, src_obj, 0);
        if (src_sym) break;
    }

    if (!src_obj) {
        xprintf("undefined symbol \"%s\" referenced in %s", name, si->name);
        ldso_die("copy relocation error");
    }

    void* src_addr = (void*)(src_obj->relocbase + src_sym->st_value);
    memcpy(dest_addr, src_addr, size);

    /* xprintf("COPY %s %x in %s -> %x in %s, size %d\n", name, src_addr, */
    /*         src_obj->name, dest_addr, si->name, size); */
    return 0;
}

int ldso_do_copy_relocations(struct so_info* si)
{
    if (si->rel) {
        ElfW(Rel) * rel;

        for (rel = si->rel; rel < si->relend; rel++) {
            if (ELFW(R_TYPE)(rel->r_info) == R_TYPE(COPY)) {
                ElfW(Rela) rela;
                rela.r_offset = rel->r_offset;
                rela.r_info = rel->r_info;
                rela.r_addend = 0;
                if (ldso_do_copy_relocation(si, &rela) == -1) return -1;
            }
        }
    }

    if (si->rela) {
        const ElfW(Rela) * rela;
        for (rela = si->rela; rela < si->relaend; rela++) {
            if (ELFW(R_TYPE)(rela->r_info) == R_TYPE(COPY)) {
                if (ldso_do_copy_relocation(si, rela) == -1) return -1;
            }
        }
    }

    return 0;
}

int ldso_relocate_objects(struct so_info* first, int bind_now)
{
    struct so_info* si;
    for (si = first; si != NULL; si = si->next) {
        if (ldso_relocate_plt_lazy(si) != 0) return -1;

        if (ldso_relocate_nonplt_objects(si) != 0) return -1;

        if (si->pltgot) ldso_setup_pltgot(si);
    }

    return 0;
}
