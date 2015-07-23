#include <elf.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "ldso.h"
#include "env.h"

#define roundup(x) ((x % pagesz == 0) ? x : ((x + pagesz) - (x % pagesz)))
#define rounddown(x) (x - (x % pagesz))

struct so_info* ldso_map_object(char* pathname, int fd)
{
	struct so_info* si = alloc_info(pathname);
	if (!si) return NULL;

	si->ehdr = mmap(NULL, pagesz, PROT_READ, MAP_SHARED, fd, 0);
	if (si->ehdr == MAP_FAILED) {
		goto failed;
	}

	if (si->ehdr->e_ident[0] != 0x7f) {
		xprintf("%s: bad elf magic number\n", pathname);
		goto failed;
	}

	if (si->ehdr->e_type != ET_EXEC && si->ehdr->e_type != ET_DYN) {
		xprintf("%s: bad elf type %x\n", pathname, si->ehdr->e_type);
		goto failed;
	}
	si->is_dynamic = (si->ehdr->e_type == ET_DYN);

	Elf32_Ehdr* ehdr = si->ehdr;
	Elf32_Phdr* phdr = (Elf32_Phdr*)((char*)ehdr + ehdr->e_phoff);
	size_t phsize = ehdr->e_phnum * sizeof(Elf32_Phdr);
	char* phend = (char*)phdr + phsize;

	Elf32_Phdr* segs[2];
	int nsegs = 0;
	for (; phdr < phend; phdr++) {
		switch (phdr->p_type) {
		case PT_LOAD:
			if (nsegs < 2) segs[nsegs] = phdr;
			nsegs++;
			break;
		case PT_DYNAMIC:
			si->dynamic = (Elf32_Dyn*) phdr->p_vaddr;
			break;
		}
	}

	if (!si->dynamic) {
		xprintf("%s: not dynamically linked\n", pathname);
		goto failed;
	}

	si->entry = (char*) ehdr->e_entry;

	if (nsegs != 2) {
		xprintf("%s: wrong number of segments\n", pathname);
		goto failed;
	}

	Elf32_Addr base_addr = si->is_dynamic ? NULL : rounddown(segs[0]->p_vaddr);
	size_t text_size = roundup(segs[0]->p_memsz);
	off_t base_offset = rounddown(segs[0]->p_offset);

	if (base_offset < pagesz) {
		munmap(ehdr, pagesz);
		ehdr = MAP_FAILED;
	}

	/* Map text segment */
	char* mapbase = mmap(base_addr, text_size, PROT_READ | PROT_EXEC, MAP_PRIVATE | (si->is_dynamic ? 0 : MAP_FIXED), fd, base_offset);
	if (mapbase == MAP_FAILED) {
		xprintf("%s: failed to map text segment\n", pathname);
		goto failed;
	}

	/* Map data segment */
	Elf32_Addr data_vaddr = rounddown(segs[1]->p_vaddr);
	size_t data_size = roundup(segs[1]->p_memsz);
	off_t data_offset = rounddown(segs[1]->p_offset);
	Elf32_Addr data_addr = mapbase + (data_vaddr - base_addr);
	Elf32_Addr clear_vaddr = data_vaddr + segs[1]->p_filesz;
	size_t clear_size = segs[1]->p_memsz - segs[1]->p_filesz;
	Elf32_Addr clear_addr = mapbase + (clear_vaddr - base_addr);

	if (mmap(data_addr, data_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd, data_offset) == MAP_FAILED) {
		xprintf("%s: failed to map data segment\n", pathname);
		goto failed;
	}

	memset(clear_addr, 0, clear_size);

	si->mapbase = mapbase;
	si->relocbase = mapbase - base_addr;

	if (si->dynamic) si->dynamic = (Elf32_Dyn*) (si->relocbase + (Elf32_Addr) si->dynamic);
	if (si->entry) si->entry = (Elf32_Dyn*) (si->relocbase + (Elf32_Addr) si->entry);

	return si;

failed:
	if (ehdr != MAP_FAILED) munmap(ehdr, pagesz);
	si->flags = 0;
	return NULL;
}
