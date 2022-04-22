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
#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "lyos/const.h"
#include "string.h"
#include <kernel/proc.h>
#include <kernel/global.h>
#include <kernel/proto.h>
#include <lyos/log.h>
#include <asm/proto.h>
#include <asm/const.h>
#include <asm/smp.h>
#include <asm/byteorder.h>
#include <asm/fixmap.h>
#include <asm/pagetable.h>
#include <asm/mach.h>
#include <asm/sysreg.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

u64 boot_args[4] __attribute__((section("data..cacheline_aligned")));

phys_bytes __fdt_pointer;
static phys_bytes phys_initrd_start, phys_initrd_size;
static int dt_root_addr_cells, dt_root_size_cells;

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];

extern char k_stacks_start;
void* k_stacks;

static char* env_get(const char* name);
static int kinfo_set_param(char* buf, char* name, char* value);

static int fdt_scan_root(void* blob, unsigned long offset, const char* name,
                         int depth, void* arg)
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

static int fdt_scan_memory(void* blob, unsigned long offset, const char* name,
                           int depth, void* arg)
{
    const char* type = fdt_getprop(blob, offset, "device_type", NULL);
    if (!type || strcmp(type, "memory") != 0) return 0;

    const u32 *reg, *lim;
    int len;
    reg = fdt_getprop(blob, offset, "reg", &len);
    if (!reg) return 0;

    lim = reg + (len / sizeof(u32));

    while ((int)(lim - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
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

/* Scan for chosen node in FDT */
static int fdt_scan_chosen(void* blob, unsigned long offset, const char* name,
                           int depth, void* arg)
{
    u64 start, end;
    const u32* prop;
    int len;

    if (depth != 1 || !arg || (strcmp(name, "chosen") != 0)) return 0;

    prop = fdt_getprop(blob, offset, "linux,initrd-start", &len);
    if (!prop) return 0;
    start = of_read_number(prop, len >> 2);

    prop = fdt_getprop(blob, offset, "linux,initrd-end", &len);
    if (!prop) return 0;
    end = of_read_number(prop, len >> 2);

    phys_initrd_start = start;
    phys_initrd_size = end - start;

    prop = fdt_getprop(blob, offset, "bootargs", &len);
    if (prop && len > 0) {
        strlcpy(arg, prop, min(len, KINFO_CMDLINE_LEN));
    }

    return 0;
}

const struct machine_desc* setup_machine_fdt(void* dtb_virt)
{
    const struct machine_desc *mdesc, *mdesc_best = NULL;
    unsigned int score = 0, best_score = ~1;

    for_each_machine_desc(mdesc)
    {
        score = of_flat_dt_match(dtb_virt, 0, mdesc->dt_compat);
        if (score > 0 && score < best_score) {
            mdesc_best = mdesc;
            best_score = score;
        }
    }

    return mdesc_best;
}

void cstart(phys_bytes dtb_phys)
{
    static char cmdline[KINFO_CMDLINE_LEN];
    static char var[KINFO_CMDLINE_LEN];
    static char value[KINFO_CMDLINE_LEN];
    phys_bytes dtb_lim;
    int fdt_size;

    swapper_pg_dir = init_pg_dir;
    k_stacks = &k_stacks_start;

    early_fixmap_init();

    initial_boot_params =
        fixmap_remap_fdt(dtb_phys, &fdt_size, ARM64_PG_KERNEL);
    dtb_lim = roundup(dtb_phys + fdt_size, ARCH_PG_SIZE);

    machine_desc = setup_machine_fdt(initial_boot_params);

    of_scan_fdt(fdt_scan_root, NULL, initial_boot_params);
    of_scan_fdt(fdt_scan_memory, NULL, initial_boot_params);

    flush_tlb();
    write_sysreg(__pa_symbol(mm_pg_dir), ttbr0_el1);
    isb();

    /* setup boot modules */
#define SET_MODULE(nr, name)                                            \
    do {                                                                \
        extern char _bootmod_##name##_start[], _bootmod_##name##_end[]; \
        kinfo.modules[nr].start_addr =                                  \
            __pa_symbol((void*)*(&_bootmod_##name##_start));            \
        kinfo.modules[nr].end_addr =                                    \
            __pa_symbol((void*)*(&_bootmod_##name##_end));              \
    } while (0)

    SET_MODULE(TASK_MM, mm);
    SET_MODULE(TASK_PM, pm);
    SET_MODULE(TASK_SERVMAN, servman);
    SET_MODULE(TASK_DEVMAN, devman);
    SET_MODULE(TASK_SCHED, sched);
    SET_MODULE(TASK_FS, vfs);
    SET_MODULE(TASK_SYS, systask);
    SET_MODULE(TASK_TTY, tty);
    SET_MODULE(TASK_RD, ramdisk);
    SET_MODULE(TASK_INITFS, initfs);
    SET_MODULE(TASK_SYSFS, sysfs);
    SET_MODULE(TASK_IPC, ipc);
    SET_MODULE(TASK_NETLINK, netlink);
    SET_MODULE(INIT, init);

    /* kernel memory layout */
    kinfo.kernel_text_start = (void*)*(&_text);
    kinfo.kernel_data_start = (void*)*(&_data);
    kinfo.kernel_bss_start = (void*)*(&_bss);
    kinfo.kernel_text_end = (void*)*(&_etext);
    kinfo.kernel_data_end = (void*)*(&_edata);
    kinfo.kernel_bss_end = (void*)*(&_ebss);

    kinfo.kernel_start_phys = __pa_symbol(&_text);
    kinfo.kernel_end_phys = __pa_symbol(&_end);

    /* reserve memory used by the kernel */
    cut_memmap(&kinfo, kinfo.kernel_start_phys, kinfo.kernel_end_phys);
    cut_memmap(&kinfo, 0, ARCH_PG_SIZE);

    paging_init();

    memset(cmdline, 0, sizeof(cmdline));
    of_scan_fdt(fdt_scan_chosen, cmdline, initial_boot_params);

    char* p = cmdline;
    while (*p) {
        int var_i = 0;
        int value_i = 0;
        while (*p == ' ')
            p++;
        if (!*p) break;
        while (*p && *p != '=' && *p != ' ' && var_i < KINFO_CMDLINE_LEN - 1)
            var[var_i++] = *p++;
        var[var_i] = 0;
        if (*p++ != '=') continue;
        while (*p && *p != ' ' && value_i < KINFO_CMDLINE_LEN - 1)
            value[value_i++] = *p++;
        value[value_i] = 0;

        kinfo_set_param(kinfo.cmdline, var, value);
    }

    char* hz_value = env_get("hz");
    if (hz_value) system_hz = strtol(hz_value, NULL, 10);
    if (!hz_value || system_hz < 2 || system_hz > 5000) system_hz = DEFAULT_HZ;

    char param_buf[64];

    cut_memmap(&kinfo, phys_initrd_start,
               roundup(phys_initrd_start + phys_initrd_size, ARCH_PG_SIZE));
    sprintf(param_buf, "0x%lx", (uintptr_t)__va(phys_initrd_start));
    kinfo_set_param(kinfo.cmdline, "initrd_base", param_buf);
    sprintf(param_buf, "%lu", (unsigned int)phys_initrd_size);
    kinfo_set_param(kinfo.cmdline, "initrd_len", param_buf);

    cut_memmap(&kinfo, dtb_phys, dtb_lim);
    sprintf(param_buf, "0x%lx", (uintptr_t)initial_boot_params);
    kinfo_set_param(kinfo.cmdline, "boot_params_base", param_buf);
    sprintf(param_buf, "%lu", (unsigned int)fdt_totalsize(initial_boot_params));
    kinfo_set_param(kinfo.cmdline, "boot_params_len", param_buf);
}

static char* get_value(const char* param, const char* key)
{
    char* envp = (char*)param;
    const char* name = key;

    for (; *envp != 0;) {
        for (name = key; *name != 0 && *name == *envp; name++, envp++)
            ;
        if (*name == '\0' && *envp == '=') return envp + 1;
        while (*envp++ != 0)
            ;
    }

    return NULL;
}

static char* env_get(const char* name)
{
    return get_value(kinfo.cmdline, name);
}

static int kinfo_set_param(char* buf, char* name, char* value)
{
    char* p = buf;
    char* bufend = buf + KINFO_CMDLINE_LEN;
    char* q;
    int namelen = strlen(name);
    int valuelen = strlen(value);

    while (*p) {
        if (strncmp(p, name, namelen) == 0 && p[namelen] == '=') {
            q = p;
            while (*q)
                q++;
            for (q++; q < bufend; q++, p++)
                *p = *q;
            break;
        }
        while (*p++)
            ;
        p++;
    }

    for (p = buf; p < bufend && (*p || *(p + 1)); p++)
        ;
    if (p > buf) p++;

    if (p + namelen + valuelen + 3 > bufend) return -1;

    strcpy(p, name);
    p[namelen] = '=';
    strcpy(p + namelen + 1, value);
    p[namelen + valuelen + 1] = 0;
    p[namelen + valuelen + 2] = 0;
    return 0;
}

void init_arch()
{
    if (machine_desc->init_machine) machine_desc->init_machine();
}
