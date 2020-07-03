#include <elf.h>

#include "ldso.h"

void ldso_bind_entry();

void ldso_setup_pltgot(struct so_info* si)
{
    si->pltgot[1] = (Elf32_Addr)si;
    si->pltgot[2] = (Elf32_Addr)ldso_bind_entry;
}

int ldso_relocate_plt_lazy(struct so_info* si)
{
    Elf32_Rel* rel;

    if (si->relocbase == NULL) return 0;

    for (rel = si->pltrel; rel < si->pltrelend; rel++) {
        Elf32_Addr* addr = (Elf32_Addr*)((char*)si->relocbase + rel->r_offset);

        *addr += (Elf32_Addr)si->relocbase;
    }

    return 0;
}

int ldso_relocate_plt_object(struct so_info* si, Elf32_Rel* rel,
                             Elf32_Addr* new_addr)
{
    Elf32_Addr* where = (Elf32_Addr*)(si->relocbase + rel->r_offset);
    unsigned long info = rel->r_info;

    Elf32_Sym* sym;

    struct so_info* obj;
    sym = ldso_find_plt_sym(si, ELF32_R_SYM(info), &obj);
    if (!sym) return -1;

    Elf32_Addr addr = (Elf32_Addr)(obj->relocbase + sym->st_value);

    if (*where != addr) *where = addr;
    if (new_addr) *new_addr = addr;
    // xprintf("%s -> %x\n", obj->strtab + sym->st_name, addr);

    return 0;
}

int ldso_relocate_nonplt_objects(struct so_info* si)
{
    Elf32_Rel* rel;

    if (si->rel) {
        for (rel = si->rel; rel < si->relend; rel++) {
            Elf32_Addr* where =
                (Elf32_Addr*)((char*)si->relocbase + rel->r_offset);
            unsigned long symnum = ELF32_R_SYM(rel->r_info);
            Elf32_Sym* sym;
            struct so_info* def_obj = NULL;

            switch (ELF32_R_TYPE(rel->r_info)) {
            case R_386_PC32:
                sym = ldso_find_sym(si, symnum, &def_obj, 0);
                *where += (Elf32_Addr)(def_obj->relocbase + sym->st_value) -
                          (Elf32_Addr)where;
                break;
            case R_386_32:
            case R_386_GLOB_DAT:
                sym = ldso_find_sym(si, symnum, &def_obj, 0);
                if (!sym) continue;
                *where += (Elf32_Addr)(def_obj->relocbase + sym->st_value);
                /* xprintf("GLOB_DAT: %s in %s -> %x in %s\n", */
                /*         (def_obj->strtab + sym->st_name), si->name, *where,
                 */
                /*         def_obj->name); */
                break;
            case R_386_RELATIVE:
                *where += (Elf32_Addr)si->relocbase;
                break;
            case R_386_COPY:
                if (si->is_dynamic) {
                    ldso_die("copy relocation in shared library");
                }
                break;
            default:
                xprintf("Unknown relocation type: %d\n",
                        ELF32_R_TYPE(rel->r_info));
                break;
            }
        }
    }

    return 0;
}

char* ldso_bind(struct so_info* si, Elf32_Word reloff)
{
    Elf32_Rel* rel = (Elf32_Rel*)((char*)si->pltrel + reloff);
    Elf32_Addr new_addr;
    int retval = ldso_relocate_plt_object(si, rel, &new_addr);

    if (retval) ldso_die("can't lookup symbol\n");

    return (char*)new_addr;
}
