#include <elf.h>

#include "ldso.h"

int ldso_relocate_objects(struct so_info* first, int bind_now)
{
	struct so_info* si;
	for (si = first; si != NULL; si = si->next) {

		if (ldso_relocate_plt_lazy(si) != 0) return -1;

		if (si->pltgot) ldso_setup_pltgot(si);
	}

	return 0;
}
