#include <stdio.h>
#include <elf.h>

#include "ldso.h"

#define MAX_NEEDED_ENTRIES	64
static struct needed_entry needed_entries[MAX_NEEDED_ENTRIES];
static int n_ent_count = 0;

static struct needed_entry* alloc_needed_entry()
{
	if (n_ent_count >= MAX_NEEDED_ENTRIES) ldso_die("too many needed objects");
	return &needed_entries[n_ent_count++];
}

int ldso_process_dynamic(struct so_info* si)
{
	Elf32_Dyn* dp;
	int use_pltrel, use_pltrela;
	Elf32_Addr pltrel, pltrelsz;
	Elf32_Addr relsz;
	Elf32_Addr init, fini;
	struct needed_entry* needed;
	struct needed_entry** needed_tail = &si->needed;

	si->needed = NULL;

	for (dp = si->dynamic; dp->d_tag != DT_NULL; dp++) {
		switch (dp->d_tag) {
		case DT_REL:
			si->rel = (Elf32_Rel*) (si->relocbase + dp->d_un.d_ptr);
			break;
		case DT_RELSZ:
			relsz = dp->d_un.d_val;
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
			si->symtab = (Elf32_Sym*) (si->relocbase + dp->d_un.d_ptr);
			break;
		case DT_STRTAB:
			si->strtab = (char*) (si->relocbase + dp->d_un.d_ptr);
			break;
		case DT_STRSZ:
			si->strtabsz = dp->d_un.d_val;
			break;
		case DT_PLTGOT:
			si->pltgot = (Elf32_Addr*) (si->relocbase + dp->d_un.d_ptr);
			break;
		case DT_INIT:
			init = dp->d_un.d_ptr;
			break;
		case DT_FINI:
			fini = dp->d_un.d_ptr;
			break;
		case DT_NEEDED:
			needed = alloc_needed_entry();
			needed->name = dp->d_un.d_val;
			needed->si = NULL;
			needed->next = NULL;

			*needed_tail = needed;
			needed_tail = &needed->next;
			break;
		case DT_HASH:
			{
				int* hash_table = (int*)(si->relocbase + dp->d_un.d_ptr);

				si->nbuckets = hash_table[0];
				si->nchains = hash_table[1];
				si->buckets = hash_table + 2;
				si->chains = si->buckets + si->nbuckets;
				break;
			}
		}
	}

	si->relend = (Elf32_Rel*) ((char*) si->rel + relsz);
	if (use_pltrel) {
		si->pltrel = (Elf32_Rel*) (si->relocbase + pltrel);
		si->pltrelend = (Elf32_Rel*) (si->relocbase + pltrel + pltrelsz);
	}

	if (init) {
		si->init = (void (*)(void)) (si->relocbase + init);
	}
	if (fini) {
		si->fini = (void (*)(void)) (si->relocbase + fini);
	}
}

int ldso_process_phdr(struct so_info* si, Elf32_Phdr* phdr, int phnum)
{
	Elf32_Phdr* hdr;

	for (hdr = phdr; hdr < phdr + phnum; hdr++) {
		if (hdr->p_type == PT_PHDR) {
			si->phdr = (char*)hdr->p_vaddr;
			si->relocbase = (char*)((char*)phdr - (char*)si->phdr);
			si->phnum = si->phnum;
			break;
		}
	}

	for (hdr = phdr; hdr < phdr + phnum; hdr++) {
		Elf32_Addr vaddr = (Elf32_Addr) (si->relocbase + hdr->p_vaddr); 
		if (hdr->p_type == PT_DYNAMIC) {
			si->dynamic = (Elf32_Dyn*) vaddr;
		}
	}
}
