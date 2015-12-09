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
#include <lyos/log.h>
#include "arch.h"
#include "arch_proto.h"
#include "arch_const.h"
#include "setup.h"

extern char _PHYS_BASE, _VIR_BASE, _KERN_SIZE, _KERN_OFFSET;
extern char _arch_init_start, _arch_init_end;

/* paging utilities */
PRIVATE phys_bytes kern_vir_base = (phys_bytes) &_VIR_BASE;
PRIVATE phys_bytes kern_phys_base = (phys_bytes) &_PHYS_BASE;
PRIVATE phys_bytes kern_size = (phys_bytes) &_KERN_SIZE;

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];
extern u32 k_stacks_start;
PUBLIC void * k_stacks;

PRIVATE char * env_get(const char *name);
PRIVATE int kinfo_set_param(char * buf, char * name, char * value);

PRIVATE void parse_atags(void* atags_ptr)
{
    struct tag* t;

    static char cmdline[KINFO_CMDLINE_LEN];
    static char var[KINFO_CMDLINE_LEN];
    static char value[KINFO_CMDLINE_LEN];

    for (t = atags_ptr; t->hdr.size; t = (struct tag *)((u32 *)t + t->hdr.size)) {
        switch (t->hdr.tag) {
        case ATAG_CORE:
            break;
        case ATAG_MEM:
            kinfo.memmaps[kinfo.memmaps_count].addr = t->u.mem.start;
            kinfo.memmaps[kinfo.memmaps_count].len = t->u.mem.size;
            kinfo.memmaps[kinfo.memmaps_count].type = KINFO_MEMORY_AVAILABLE;
            kinfo.memmaps_count++;
            kinfo.memory_size += t->u.mem.size;
            break;
        case ATAG_CMDLINE:
        {
            strlcpy(cmdline, (void *)t->u.cmdline.cmdline, KINFO_CMDLINE_LEN);
            char * p = cmdline;
            while (*p) {
                int var_i = 0;
                int value_i = 0;
                while (*p == ' ') p++;
                if (!*p) break;
                while (*p && *p != '=' && *p != ' ' && var_i < KINFO_CMDLINE_LEN - 1) 
                    var[var_i++] = *p++ ;
                var[var_i] = 0;
                if (*p++ != '=') continue;
                while (*p && *p != ' ' && value_i < KINFO_CMDLINE_LEN - 1) 
                    value[value_i++] = *p++ ;
                value[value_i] = 0;
            
                kinfo_set_param(kinfo.cmdline, var, value);
            }
        }
        default:
            break;
        }
    }
}

PUBLIC void cstart(int r0, int mach_type, void* atags_ptr)
{
    memset(&kinfo, 0, sizeof(kinfo_t));
    kinfo.magic = KINFO_MAGIC;

    kinfo.kernel_start_pde = ARCH_PDE(kern_vir_base);
    kinfo.kernel_end_pde = ARCH_PDE(kern_vir_base + kern_size);
    kinfo.kernel_start_phys = kern_phys_base;
    kinfo.kernel_end_phys = kern_phys_base + kern_size; 
    if (kinfo.kernel_end_phys % PAGE_ALIGN) {
        kinfo.kernel_end_phys += ARCH_PG_SIZE - kinfo.kernel_end_phys % PAGE_ALIGN;
        kinfo.kernel_end_pde++;
    }

    k_stacks = &k_stacks_start;

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
    
    if (mach >= (struct machine_desc*) &_arch_init_end) panic("machine not supported(mach_type: %d)", mach_type);
    machine_desc = mach;
    uart_base_addr = machine_desc->uart_base;
    
    parse_atags(atags_ptr);
    
    init_prot();
    
    memset(&kern_log, 0, sizeof(struct kern_log));
    spinlock_init(&kern_log.lock);

#define SET_MODULE(nr, name) do { \
    extern char _binary_##name##_start[], _binary_##name##_end[]; \
    kinfo.modules[nr].start_addr = (vir_bytes)*(&_binary_##name##_start) - (phys_bytes) &_KERN_OFFSET; \
    kinfo.modules[nr].end_addr = (vir_bytes)*(&_binary_##name##_end) - (phys_bytes) &_KERN_OFFSET; } while(0)

    SET_MODULE(TASK_MM, mm);
    SET_MODULE(TASK_PM, pm);
    SET_MODULE(TASK_SERVMAN, servman);

    cut_memmap(&kinfo, 0, PG_SIZE);
    cut_memmap(&kinfo, kinfo.kernel_start_phys, kinfo.kernel_end_phys);
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
    if (machine_desc->init_machine)
        machine_desc->init_machine();

    init_tss(0, get_k_stack_top(0));
}
