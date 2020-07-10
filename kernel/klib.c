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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"

#include "multiboot.h"
#include <elf.h>

/*****************************************************************************
 *                                get_kernel_map
 *****************************************************************************/
/**
 * <Ring 0~1> Parse the kernel file, get the memory range of the kernel image.
 *
 * - The meaning of `base':	base => first_valid_byte
 * - The meaning of `limit':	base + limit => last_valid_byte
 *
 * @param b   Memory base of kernel.
 * @param l   Memory limit of kernel.
 *****************************************************************************/
#if 0
int get_kernel_map(unsigned int * b, unsigned int * l)
{
	multiboot_elf_section_header_table_t * shdrs = (multiboot_elf_section_header_table_t *)kernel_file;

	*b = ~0;
	unsigned int t = 0;
	int i;
	for (i = 0; i < shdrs->num; i++) {
		Elf32_Shdr* section_header =
			(Elf32_Shdr*)(shdrs->addr +
				      i * shdrs->size);
		if (section_header->sh_flags & SHF_ALLOC) {
			int bottom = section_header->sh_addr;
			int top = section_header->sh_addr +
				section_header->sh_size;

			if (*b > bottom)
				*b = bottom;
			if (t < top)
				t = top;
		}
	}
	assert(*b < t);
	*l = t - *b - 1;

	return 0;
}
#endif

/*======================================================================*
                               delay
 *======================================================================*/
void delay(int time)
{
    int i, j, k;
    for (k = 0; k < time; k++) {
        /*for(i=0;i<10000;i++){	for Virtual PC	*/
        for (i = 0; i < 10; i++) { /*	for Bochs	*/
            for (j = 0; j < 10000; j++) {
            }
        }
    }
}
