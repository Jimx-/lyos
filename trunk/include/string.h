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

PUBLIC	void*	memcpy(void* p_dst, void* p_src, int size);
PUBLIC	void	memset(void* p_dst, char ch, int size);
PUBLIC	int	strlen(const char* p_str);
PUBLIC	int	memcmp(const void * s1, const void *s2, int n);
PUBLIC	int	strcmp(const char * s1, const char *s2);
PUBLIC	char*	strcat(char * s1, const char *s2);

/**
 * `phys_copy' and `phys_set' are used only in the kernel, where segments
 * are all flat (based on 0). In the meanwhile, currently linear address
 * space is mapped to the identical physical address space. Therefore,
 * a `physical copy' will be as same as a common copy, so does `phys_set'.
 */
#define	phys_copy	memcpy
#define	phys_set	memset

