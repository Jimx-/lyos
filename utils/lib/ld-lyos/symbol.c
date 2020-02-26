#include <elf.h>

#include "ldso.h"

static int __strcmp(char* s1, char* s2)
{
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }

    return (*(unsigned char*)s1) - (*(unsigned char*)s2);
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

Elf32_Sym* ldso_lookup_symbol_obj(char* name, unsigned long hash,
                                  struct so_info* si, int in_plt)
{
    unsigned long symnum;

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

Elf32_Sym* ldso_lookup_symbol_list(char* name, unsigned long hash,
                                   struct so_info* list, struct so_info** obj,
                                   int in_plt)
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

Elf32_Sym* ldso_lookup_symbol(char* name, unsigned long hash,
                              struct so_info* so, struct so_info** obj,
                              int in_plt)
{
    Elf32_Sym* def = NULL;
    struct so_info* def_obj = NULL;

    if (!def) {
        Elf32_Sym* sym =
            ldso_lookup_symbol_list(name, hash, si_list, &def_obj, in_plt);
        if (sym) def = sym;
    }

    if (def) {
        *obj = def_obj;
        return def;
    }

    return NULL;
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
