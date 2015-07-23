#include <elf.h>

#include "ldso.h"

void ldso_bind_entry();

void ldso_setup_pltgot(struct so_info* si)
{
	si->pltgot[1] = (Elf32_Addr) si;
	si->pltgot[2] = (Elf32_Addr) ldso_bind_entry;
}

int ldso_relocate_plt_lazy(struct so_info* si)
{
	Elf32_Rel* rel;

	if (si->relocbase == NULL) return 0;

	for (rel = si->pltrel; rel < si->pltrelend; rel++) {
		Elf32_Addr* addr = (Elf32_Addr*) ((char*) si->pltrel + rel->r_offset);

		*addr += (Elf32_Addr) si->relocbase;
	}

	return 0;
}

int ldso_relocate_plt_object(struct so_info* si, Elf32_Rel* rel, Elf32_Addr* new_addr)
{
	Elf32_Addr* where = (Elf32_Addr*) (si->relocbase + rel->r_offset);
	unsigned long info = rel->r_info;

	Elf32_Sym* sym;

	struct so_info* obj;
	sym = ldso_find_plt_sym(si, ELF32_R_SYM(info), &obj);
	if (!sym) return -1;

	Elf32_Addr addr = (Elf32_Addr)(obj->relocbase + sym->st_value);

	if (*where != addr) *where = addr;
	if (new_addr) *new_addr = addr;

	return 0;
}

char* ldso_bind(struct so_info* si, Elf32_Word reloff)
{
	Elf32_Rel* rel = (Elf32_Rel*) ((char*) si->pltrel + reloff);

	Elf32_Addr new_addr;
	int retval = ldso_relocate_plt_object(si, rel, &new_addr);

	if (retval) ldso_die("can't lookup symbol\n");

	return (char*) new_addr;
}
