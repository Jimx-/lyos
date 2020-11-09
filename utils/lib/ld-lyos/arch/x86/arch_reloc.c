#include <elf.h>

#include "ldso.h"

void ldso_bind_entry();

void ldso_relocate_nonplt_self(Elf32_Dyn* dynp, Elf32_Addr relocbase)
{
    const Elf32_Rel *rel, *rel_lim;
    Elf32_Addr rel_size;
    Elf32_Addr* where;

    for (; dynp->d_tag != DT_NULL; dynp++) {
        switch (dynp->d_tag) {
        case DT_REL:
            rel = (const Elf32_Rel*)(relocbase + dynp->d_un.d_ptr);
            break;
        case DT_RELSZ:
            rel_size = dynp->d_un.d_val;
            break;
        }
    }

    if (!rel || !rel_size) return;

    rel_lim = (const Elf32_Rel*)((char*)rel + rel_size);

    for (; rel < rel_lim; rel++) {
        where = (Elf32_Addr*)(relocbase + rel->r_offset);
        *where += (Elf32_Addr)relocbase;
    }
}

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
    /* xprintf("%s -> %x\n", obj->strtab + sym->st_name, addr); */

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
            case R_386_NONE:
                break;
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

#if defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2)
            case R_386_TLS_TPOFF:
                sym = ldso_find_sym(si, symnum, &def_obj, 0);
                if (!sym) continue;

                if (!def_obj->tls_done && ldso_tls_allocate_offset(def_obj))
                    return -1;

                *where += (Elf32_Addr)(sym->st_value - def_obj->tls_offset);
                /* xprintf("TLS_TPOFF: %s in %s -> %p\n", */
                /*         (def_obj->strtab + sym->st_name), si->name, *where);
                 */
                break;

            case R_386_TLS_DTPMOD32:
                sym = ldso_find_sym(si, symnum, &def_obj, 0);
                if (!sym) return -1;

                *where = (Elf32_Addr)(def_obj->tls_index);

                /* xprintf("TLS_DTPMOD32 %s in %s -> %p\n", */
                /*         def_obj->strtab + sym->st_name, si->name, *where); */
                break;

#endif
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

    if (retval) {
        Elf32_Sym* sym = si->symtab + ELF32_R_SYM(rel->r_info);
        char* name = si->strtab + sym->st_name;

        xprintf("can't lookup symbol %s\n", name);
        ldso_die("bind");
    }

    return (char*)new_addr;
}

#if defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2)

LDSO_PUBLIC __attribute__((__regparm__(1))) void* ___tls_get_addr(void* arg_)
{
    size_t* arg = (size_t*)arg_;
    struct tls_tcb* tcb = __libc_get_tls_tcb();
    size_t idx = arg[0], offset = arg[1];
    void** dtv = tcb->tcb_dtv;

    if (idx < (size_t)dtv[-1] && dtv[idx] != NULL)
        return (uint8_t*)dtv[idx] + offset;

    return ldso_tls_get_addr(tcb, idx, offset);
}

#endif
