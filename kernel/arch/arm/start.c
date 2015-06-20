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
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "arch.h"

extern char _PHYS_BASE, _VIR_BASE, _KERN_SIZE;
extern char _arch_init_start, _arch_init_end;

/* paging utilities */
PRIVATE phys_bytes kern_vir_base = (phys_bytes) &_VIR_BASE;
PRIVATE phys_bytes kern_phys_base = (phys_bytes) &_PHYS_BASE;
PRIVATE phys_bytes kern_size = (phys_bytes) &_KERN_SIZE;

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];
PUBLIC void * k_stacks;

PRIVATE char * env_get(const char *name);
PRIVATE int kinfo_set_param(char * buf, char * name, char * value);

PUBLIC void cstart(int r0, int mach_type, void* atags_ptr)
{
    /*
    memset(kinfo.cmdline, 0, sizeof(kinfo.cmdline));
    int i;
    for (i = 1; i < argc; i++) {
        \/* copy argument into kinfo cmdline buffer *\/
        char* name = argv[i];
        char* value = strchr(name, '=');
        if (!value) continue;

        *value = '\0';
        value++;
        kinfo_set_param(kinfo.cmdline, name, value);
    }*/

    kinfo.kernel_start_pde = ARCH_PDE(kern_vir_base);
    kinfo.kernel_end_pde = ARCH_PDE(kern_vir_base + kern_size);
    kinfo.kernel_start_phys = kern_phys_base;
    kinfo.kernel_end_phys = kern_phys_base + kern_size; 

    /* kernel memory layout */
    kinfo.kernel_text_start = (vir_bytes)*(&_text);
    kinfo.kernel_data_start = (vir_bytes)*(&_data);
    kinfo.kernel_bss_start = (vir_bytes)*(&_bss);
    kinfo.kernel_text_end = (vir_bytes)*(&_etext);
    kinfo.kernel_data_end = (vir_bytes)*(&_edata);
    kinfo.kernel_bss_end = (vir_bytes)*(&_ebss);

    char * hz_value = env_get("hz");
    if (hz_value) system_hz = atoi(hz_value);
    if (!hz_value || system_hz < 2 || system_hz > 5000) system_hz = DEFAULT_HZ;

    struct machine_desc* mach = (struct machine_desc*) &_arch_init_start;
    for (; mach < (struct machine_desc*) &_arch_init_end; mach++) {
        if (mach->id == mach_type) break;
    }
    if (mach >= (struct machine_desc*) &_arch_init_end) panic("machine not supported");
    direct_print("Mach type: %s\n", mach->name);
    
    while(1);
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
