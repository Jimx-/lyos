#define _GNU_SOURCE

#include <elf.h>
#include <dlfcn.h>

#include "ldso.h"

static int __strcmp(const char* s1, const char* s2)
{
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }

    return (*(unsigned char*)s1) - (*(unsigned char*)s2);
}

static int ldso_is_exported(const Elf32_Sym* sym)
{
    static const void* ldso_exports[] = {dlopen, dlsym, __ldso_allocate_tls,
                                         NULL};
    int i;
    void* value;

    value = (void*)(si_self.relocbase + sym->st_value);
    for (i = 0; ldso_exports[i]; i++) {
        if (ldso_exports[i] == value) return 1;
    }

    return 0;
}

unsigned long ldso_elf_hash(const char* name)
{
    const unsigned char* p = (const unsigned char*)name;
    unsigned long h = 0;
    unsigned long g;
    unsigned long c;

    for (; (c = *p) != '\0'; p++) {
        h <<= 4;
        h += c;
        if ((g = h & 0xf0000000) != 0) {
            h ^= g;
            h ^= g >> 24;
        }
    }
    return h;
}

Elf32_Sym* ldso_lookup_symbol_obj(const char* name, unsigned long hash,
                                  struct so_info* si, int in_plt)
{
    unsigned long symnum;

    if (!si->nbuckets) {
        return NULL;
    }

    for (symnum = si->buckets[hash % si->nbuckets]; symnum != 0;
         symnum = si->chains[symnum]) {

        Elf32_Sym* sym = si->symtab + symnum;
        char* str = si->strtab + sym->st_name;

        if (__strcmp(name, str)) continue;

        if (sym->st_shndx == SHN_UNDEF &&
            (in_plt || sym->st_value == 0 ||
             ELF32_ST_TYPE(sym->st_info) != STT_FUNC))
            continue;

        return sym;
    }

    return NULL;
}

static Elf32_Sym* ldso_lookup_symbol_list(const char* name, unsigned long hash,
                                          struct so_info* list,
                                          struct so_info** obj, int in_plt)
{
    struct so_info* si;
    Elf32_Sym* sym;
    for (si = list; si != NULL; si = si->next) {
        sym = ldso_lookup_symbol_obj(name, hash, si, in_plt);

        if (sym) {
            *obj = si;
            return sym;
        }
    }

    return NULL;
}

static Elf32_Sym* ldso_lookup_symbol(const char* name, unsigned long hash,
                                     struct so_info* so, struct so_info** obj,
                                     int in_plt)
{
    Elf32_Sym* def = NULL;
    Elf32_Sym* sym = NULL;
    struct so_info* def_obj = NULL;

    if (!def) {
        sym = ldso_lookup_symbol_list(name, hash, si_list, &def_obj, in_plt);
        if (sym) def = sym;
    }

    if (!def || ELF32_ST_BIND(def->st_info) == STB_WEAK) {
        sym = ldso_lookup_symbol_obj(name, hash, &si_self, in_plt);

        if (sym && ldso_is_exported(sym)) {
            def = sym;
            def_obj = &si_self;
        }
    }

    if (def) {
        *obj = def_obj;
    }

    return def;
}

Elf32_Sym* ldso_find_sym(struct so_info* si, unsigned long symnum,
                         struct so_info** obj, int in_plt)
{
    Elf32_Sym* sym;
    Elf32_Sym* def = NULL;
    struct so_info* def_obj = NULL;

    sym = si->symtab + symnum;
    char* name = si->strtab + sym->st_name;

    if (ELF32_ST_BIND(sym->st_info) != STB_LOCAL) {
        unsigned long hash = ldso_elf_hash(name);
        def = ldso_lookup_symbol(name, hash, si, &def_obj, in_plt);
    } else {
        def = sym;
        def_obj = si;
    }

    if (def) {
        *obj = def_obj;
    }

    return def;
}

Elf32_Sym* ldso_find_plt_sym(struct so_info* si, unsigned long symnum,
                             struct so_info** obj)
{
    return ldso_find_sym(si, symnum, obj, 1);
}

void* do_dlsym(void* handle, const char* name, void* retaddr)
{
    unsigned long hash;
    Elf32_Sym* def;
    struct so_info *si, *def_obj;

    def = NULL;
    def_obj = NULL;
    hash = ldso_elf_hash(name);

    switch ((intptr_t)handle) {
    case (intptr_t)RTLD_NEXT:
    case (intptr_t)RTLD_DEFAULT:
        if ((si = ldso_get_obj_from_addr(retaddr)) == NULL) return NULL;

        switch ((intptr_t)handle) {
        case (intptr_t)RTLD_NEXT:
            si = si->next;
            for (; si != NULL; si = si->next) {
                if ((def = ldso_lookup_symbol_obj(name, hash, si, 1)) != NULL)
                    def_obj = si;
            }
            break;
        case (intptr_t)RTLD_DEFAULT:
            def = ldso_lookup_symbol(name, hash, si, &def_obj, 1);
            break;
        }
        break;
    default:
        if ((si = ldso_check_handle(handle)) == NULL) return NULL;

        if ((def = ldso_lookup_symbol_obj(name, hash, si, 1)) != NULL)
            def_obj = si;
        break;
    }

    if (def != NULL) {
        void* p;
        p = def_obj->relocbase + def->st_value;
        return p;
    }

    return NULL;
}

void* dlsym(void* handle, const char* name)
{
    void* retaddr;

    retaddr = __builtin_return_address(0);
    return do_dlsym(handle, name, retaddr);
}
