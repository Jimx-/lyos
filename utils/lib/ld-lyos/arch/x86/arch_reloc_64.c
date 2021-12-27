#include <elf.h>
#include <stddef.h>

#include "ldso.h"

void ldso_bind_entry();

void ldso_relocate_nonplt_self(ElfW(Dyn) * dynp, ElfW(Addr) relocbase)
{
    const ElfW(Rela) * rela, *rela_lim;
    ElfW(Addr) rela_size;
    ElfW(Addr) * where;

    rela = NULL;

    for (; dynp->d_tag != DT_NULL; dynp++) {
        switch (dynp->d_tag) {
        case DT_RELA:
            rela = (const ElfW(Rela)*)(relocbase + dynp->d_un.d_ptr);
            break;
        case DT_RELASZ:
            rela_size = dynp->d_un.d_val;
            break;
        }
    }

    if (!rela || !rela_size) return;

    rela_lim = (const ElfW(Rela)*)((char*)rela + rela_size);

    for (; rela < rela_lim; rela++) {
        ElfW(Word) r_type = ELFW(R_TYPE)(rela->r_info);
        ElfW(Addr)* where = (ElfW(Addr)*)(relocbase + rela->r_offset);

        switch (r_type) {
        case R_X86_64_RELATIVE:
            *where = (ElfW(Addr))relocbase + rela->r_addend;
            break;
        }
    }
}

void ldso_setup_pltgot(struct so_info* si)
{
    si->pltgot[1] = (ElfW(Addr))si;
    si->pltgot[2] = (ElfW(Addr))ldso_bind_entry;
}

int ldso_relocate_plt_lazy(struct so_info* si)
{
    ElfW(Rela) * rela;

    if (si->relocbase == NULL) return 0;

    for (rela = si->pltrela; rela < si->pltrelaend; rela++) {
        ElfW(Addr)* addr = (ElfW(Addr)*)((char*)si->relocbase + rela->r_offset);

        *addr += (ElfW(Addr))si->relocbase;
    }

    return 0;
}

int ldso_relocate_plt_object(struct so_info* si, ElfW(Rela) * rel,
                             ElfW(Addr) * new_addr)
{
    ElfW(Addr)* where = (ElfW(Addr)*)(si->relocbase + rel->r_offset);
    unsigned long info = rel->r_info;

    ElfW(Sym) * sym;

    struct so_info* obj;
    sym = ldso_find_plt_sym(si, ELFW(R_SYM)(info), &obj);
    if (!sym) return -1;

    ElfW(Addr) addr = (ElfW(Addr))(obj->relocbase + sym->st_value);

    if (*where != addr) *where = addr;
    if (new_addr) *new_addr = addr;
    /* xprintf("%s -> %x\n", obj->strtab + sym->st_name, addr); */

    return 0;
}

int ldso_relocate_nonplt_objects(struct so_info* si)
{
    ElfW(Rela) * rela;

    if (si->rela) {
        for (rela = si->rela; rela < si->relaend; rela++) {
            ElfW(Word) r_type = ELFW(R_TYPE)(rela->r_info);
            ElfW(Word) symnum = ELFW(R_SYM)(rela->r_info);
            ElfW(Addr)* where = (ElfW(Addr)*)(si->relocbase + rela->r_offset);
            ElfW(Sym) * sym;
            struct so_info* def_obj = NULL;

            switch (r_type) {
            case R_X86_64_NONE:
                break;
            case R_X86_64_64:
                sym = ldso_find_sym(si, symnum, &def_obj, 0);
                if (!sym) continue;

                *where = (ElfW(Addr))def_obj->relocbase + sym->st_value +
                         rela->r_addend;
                break;
            case R_X86_64_GLOB_DAT:
                sym = ldso_find_sym(si, symnum, &def_obj, 0);
                if (!sym) continue;
                *where = (ElfW(Addr))(def_obj->relocbase + sym->st_value);
                /* xprintf("GLOB_DAT: %s in %s -> %x in %s\n", */
                /*         (def_obj->strtab + sym->st_name), si->name, *where,
                 */
                /*         def_obj->name); */
                break;
            case R_X86_64_RELATIVE:
                *where = (ElfW(Addr))si->relocbase + rela->r_addend;
                break;
            case R_X86_64_COPY:
                if (si->is_dynamic) {
                    ldso_die("copy relocation in shared library");
                }
                break;

#if defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2)
            case R_X86_64_TPOFF64:
                sym = ldso_find_sym(si, symnum, &def_obj, 0);
                if (!sym) continue;

                if (!def_obj->tls_done && ldso_tls_allocate_offset(def_obj))
                    return -1;

                *where = (ElfW(Addr))(sym->st_value - def_obj->tls_offset +
                                      rela->r_addend);
                break;

            case R_X86_64_DTPMOD64:
                sym = ldso_find_sym(si, symnum, &def_obj, 0);
                if (!sym) continue;

                *where = (ElfW(Addr))def_obj->tls_index;
                break;

            case R_X86_64_DTPOFF64:
                sym = ldso_find_sym(si, symnum, &def_obj, 0);
                if (!sym) continue;

                *where = (ElfW(Addr))(sym->st_value + rela->r_addend);
                break;
#endif
            default:
                xprintf("Unknown relocation type: %d\n", r_type);
                break;
            }
        }
    }

    return 0;
}

char* ldso_bind(struct so_info* si, ElfW(Word) reloff)
{
    ElfW(Rela)* rela = si->pltrela + reloff;
    ElfW(Addr) new_addr;
    int retval = ldso_relocate_plt_object(si, rela, &new_addr);

    if (retval) {
        ElfW(Sym)* sym = si->symtab + ELFW(R_SYM)(rela->r_info);
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
