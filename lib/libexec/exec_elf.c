/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "assert.h"
#include "unistd.h"
#include "errno.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <elf.h>
#include "libexec.h"

PRIVATE int elf_check_header(Elf32_Ehdr * elf_hdr);
PRIVATE int elf_unpack(char * hdr, Elf32_Ehdr ** elf_hdr, Elf32_Phdr ** prog_hdr);

PRIVATE int elf_check_header(Elf32_Ehdr * elf_hdr)
{
	if (elf_hdr->e_ident[0] != 0x7f || elf_hdr->e_type != ET_EXEC) {
		return ENOEXEC;
	}

	return 0;
}

PRIVATE int elf_unpack(char * hdr, Elf32_Ehdr ** elf_hdr, Elf32_Phdr ** prog_hdr)
{
	*elf_hdr = (Elf32_Ehdr *)hdr;
	*prog_hdr = (Elf32_Phdr *)(hdr + (*elf_hdr)->e_phoff);

	return 0;
}

PUBLIC int libexec_load_elf(struct exec_info * execi)
{
    int retval;
    Elf32_Ehdr * elf_hdr;
    Elf32_Phdr * prog_hdr;

    if ((retval = elf_unpack(execi->header, &elf_hdr, &prog_hdr)) != 0) return retval;

    
    return 0;
}
