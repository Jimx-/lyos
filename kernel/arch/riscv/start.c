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

#include <lyos/config.h>
#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/log.h>
#include <asm/proto.h>
#include <asm/const.h>
#include <asm/setup.h>
#include <asm/smp.h>
#include <asm/byteorder.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

/* counter for selecting one hart to boot the others */
unsigned int hart_counter __attribute__((__section__(".unpaged_text"))) = 0;

extern char _VIR_BASE, _KERN_SIZE;
extern char __global_pointer;

/* paging utilities */
PRIVATE phys_bytes kern_vir_base = (phys_bytes) &_VIR_BASE;
PRIVATE phys_bytes kern_size = (phys_bytes) &_KERN_SIZE;

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];
extern char k_stacks_start;
PUBLIC void * k_stacks;

PRIVATE int dt_root_addr_cells, dt_root_size_cells;

PRIVATE char * env_get(const char *name);
PRIVATE int kinfo_set_param(char * buf, char * name, char * value);

/* Scan for root address cells and size cells in FDT */
PRIVATE int fdt_scan_root(void* blob, unsigned long offset, const char* name, int depth, void* arg)
{
    if (depth != 0) return 0;

    dt_root_addr_cells = 1;
    dt_root_size_cells = 1;

    u32* prop;
    if ((prop = fdt_getprop(blob, offset, "#size-cells", NULL)) != NULL) {
        dt_root_size_cells = be32_to_cpup(prop);
    }

    if ((prop = fdt_getprop(blob, offset, "#address-cells", NULL)) != NULL) {
        dt_root_addr_cells = be32_to_cpup(prop);
    }

    return 1;
}

/* Scan for memory nodes in FDT */
PRIVATE int fdt_scan_memory(void* blob, unsigned long offset, const char* name, int depth, void* arg)
{
    const char* type = fdt_getprop(blob, offset, "device_type", NULL);
    if (!type || strcmp(type, "memory") != 0) return 0;

    const u32* reg, *lim;
    size_t len;
    reg = fdt_getprop(blob, offset, "reg", &len);
    if (!reg) return 0;

    lim = reg + (len / sizeof(u32));

    while ((int) (lim - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
        phys_bytes base, size;

        base = of_read_number(reg, dt_root_addr_cells);
        reg += dt_root_addr_cells;
        size = of_read_number(reg, dt_root_size_cells);
        reg += dt_root_size_cells;

        if (size == 0) continue;

		kinfo.memmaps[kinfo.memmaps_count].addr = base;
		kinfo.memmaps[kinfo.memmaps_count].len = size;
		kinfo.memmaps[kinfo.memmaps_count].type = KINFO_MEMORY_AVAILABLE;
		kinfo.memmaps_count++;
		kinfo.memory_size += size;
    }

    return 0;
}

PUBLIC void cstart(unsigned int hart_id, void* dtb_phys)
{
	kinfo.memmaps_count = 0;
    kinfo.memory_size = 0;

    of_scan_fdt(fdt_scan_root, NULL, __va(dtb_phys));
    of_scan_fdt(fdt_scan_memory, NULL, __va(dtb_phys));
}

PRIVATE char * get_value(const char * param, const char * key)
{
    char * envp = (char *)param;
    const char * name = key;

    for (; *envp != 0;) {
        for (name = key; *name != 0 && *name == *envp; name++, envp++);
        if (*name == '\0' && *envp == '=') return envp + 1;
        while (*envp++ != 0);
    }

    return NULL;
}

PRIVATE char * env_get(const char *name)
{
    return get_value(kinfo.cmdline, name);
}

PRIVATE int kinfo_set_param(char * buf, char * name, char * value)
{
    char *p = buf;
    char *bufend = buf + KINFO_CMDLINE_LEN;
    char *q;
    int namelen = strlen(name);
    int valuelen = strlen(value);

    while (*p) {
        if (strncmp(p, name, namelen) == 0 && p[namelen] == '=') {
            q = p;
            while (*q) q++;
            for (q++; q < bufend; q++, p++)
                *p = *q;
            break;
        }
        while (*p++);
        p++;
    }
    
    for (p = buf; p < bufend && (*p || *(p + 1)); p++);
    if (p > buf) p++;
    
    if (p + namelen + valuelen + 3 > bufend)
        return -1;
    
    strcpy(p, name);
    p[namelen] = '=';
    strcpy(p + namelen + 1, value);
    p[namelen + valuelen + 1] = 0;
    p[namelen + valuelen + 2] = 0;
    return 0;
}

PUBLIC void init_arch()
{
}
