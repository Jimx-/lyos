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
#include "stdio.h"
#include <stdlib.h>
#include <asm/protect.h>
#include "lyos/const.h"
#include "string.h"
#include <kernel/global.h>
#include "multiboot.h"
#include <asm/page.h>
#include "acpi.h"
#include <asm/const.h>
#include <asm/proto.h>
#include <lyos/log.h>
#include <lyos/spinlock.h>
#include <kernel/watchdog.h>

#ifdef CONFIG_KVM_GUEST
#include <lyos/kvm_para.h>
#endif

extern char _text[], _etext[], _data[], _edata[], _bss[], _ebss[], _end[];
void* k_stacks;

extern pde_t init_top_pgt;

static int kinfo_set_param(char* buf, char* name, char* value);
static char* env_get(const char* name);

/*======================================================================*
                            cstart
 *======================================================================*/
void cstart(struct multiboot_info* mboot, u32 mboot_magic)
{
    kinfo.memory_size = 0;
    memset(&kinfo, 0, sizeof(kinfo_t));

    va_pa_offset = KERNEL_VMA;

    kinfo.magic = KINFO_MAGIC;
    mb_magic = mboot_magic;

    void* mb_mmap_addr;
    size_t mb_mmap_len;

    /* grub provides physical address, we want virtual address */
    mboot = (struct multiboot_info*)__va(mboot);
    mb_mmap_addr = __va(mboot->mmap_addr);
    mb_mmap_len = mboot->mmap_length;

    kinfo.mods_count = mb_mod_count = mboot->mods_count;

    kinfo.memmaps_count = -1;
    struct multiboot_mmap_entry* mmap =
        (struct multiboot_mmap_entry*)mb_mmap_addr;
    while ((void*)mmap < mb_mmap_addr + mb_mmap_len) {
        kinfo.memmaps_count++;
        kinfo.memmaps[kinfo.memmaps_count].addr = mmap->addr;
        kinfo.memmaps[kinfo.memmaps_count].len = mmap->len;
        kinfo.memmaps[kinfo.memmaps_count].type = mmap->type;
        kinfo.memory_size += mmap->len;
        mmap = (struct multiboot_mmap_entry*)((uintptr_t)mmap + mmap->size +
                                              sizeof(unsigned int));
    }

    mb_mod_addr = __va(mboot->mods_addr);
    multiboot_module_t* last_mod =
        (multiboot_module_t*)(unsigned long)mboot->mods_addr;
    last_mod += mb_mod_count - 1;

    phys_bytes mod_ends = (phys_bytes)last_mod->mod_end;

    /* setup kernel page table */

    initial_pgd = &init_top_pgt;

    phys_bytes procs_base = mod_ends & ARCH_PG_MASK;
    unsigned long kernel_pts;

#ifdef CONFIG_X86_32
    procs_base = roundup(procs_base, ARCH_PGD_SIZE);
    kernel_pts = procs_base >> ARCH_PGD_SHIFT;
#else
    procs_base = roundup(procs_base, ARCH_PMD_SIZE);
    kernel_pts = procs_base >> ARCH_PMD_SHIFT;
#endif

    kinfo.kernel_start_pde = ARCH_PDE(KERNEL_VMA);
    kinfo.kernel_end_pde = ARCH_PDE(KERNEL_VMA) + kernel_pts;
    kinfo.kernel_start_phys = 0;
    kinfo.kernel_end_phys = procs_base;

    /* Identity map the first 4G so that the kernel can get access to ACPI
     * tables and HPET before MM sets up those mappings for it. */
    pg_identity_map(initial_pgd, 0, 0xffffffffUL, 0);

#ifdef CONFIG_X86_32
    pg_mapkernel(initial_pgd);
    pg_load(initial_pgd);
#endif

    init_prot();

    mb_flags = mboot->flags;

    k_stacks = &k_stacks_start;

    sysinfo_user = NULL;
    memset(&kern_log, 0, sizeof(struct kern_log));
    spinlock_init(&kern_log.lock);

    static char cmdline[KINFO_CMDLINE_LEN];
    if (mb_flags & MULTIBOOT_INFO_CMDLINE) {
        static char var[KINFO_CMDLINE_LEN];
        static char value[KINFO_CMDLINE_LEN];

        memcpy(cmdline, (void*)(unsigned long)mboot->cmdline,
               KINFO_CMDLINE_LEN);
        char* p = cmdline;
        while (*p) {
            int var_i = 0;
            int value_i = 0;
            while (*p == ' ')
                p++;
            if (!*p) break;
            while (*p && *p != '=' && *p != ' ' &&
                   var_i < KINFO_CMDLINE_LEN - 1)
                var[var_i++] = *p++;
            var[var_i] = 0;
            if (*p++ != '=') continue;
            while (*p && *p != ' ' && value_i < KINFO_CMDLINE_LEN - 1)
                value[value_i++] = *p++;
            value[value_i] = 0;

            kinfo_set_param(kinfo.cmdline, var, value);
        }
    }

    /* set initrd parameters */
    multiboot_module_t* initrd_mod = (multiboot_module_t*)mb_mod_addr;
    kinfo.initrd_base_phys = (phys_bytes)initrd_mod->mod_start;
    kinfo.initrd_len = initrd_mod->mod_end - initrd_mod->mod_start;

    /* set module information */
    int i;
    multiboot_module_t* bootmod = initrd_mod + 1;
    for (i = 0; i < NR_BOOT_PROCS - NR_TASKS; i++, bootmod++) {
        kinfo.modules[i].start_addr = (phys_bytes)bootmod->mod_start;
        kinfo.modules[i].end_addr = (phys_bytes)bootmod->mod_end;
    }
    /*
    #define SET_MODULE(nr, name) do { \
        extern char _binary_##name##_start[], _binary_##name##_end[]; \
        kinfo.modules[nr].start_addr = (void*)*(&_binary_##name##_start) -
    KERNEL_VMA; \ kinfo.modules[nr].end_addr = (void*)*(&_binary_##name##_end) -
    KERNEL_VMA; } while(0)

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
        SET_MODULE(INIT, init);
    */
    /* kernel memory layout */
    kinfo.kernel_text_start = (void*)*(&_text);
    kinfo.kernel_data_start = (void*)*(&_data);
    kinfo.kernel_bss_start = (void*)*(&_bss);
    kinfo.kernel_text_end = (void*)*(&_etext);
    kinfo.kernel_data_end = (void*)*(&_edata);
    kinfo.kernel_bss_end = (void*)*(&_ebss);

    cut_memmap(&kinfo, 0, ARCH_PG_SIZE);
    cut_memmap(&kinfo, 0x100000, kinfo.kernel_end_phys);

    char* hz_value = env_get("hz");
    if (hz_value) system_hz = strtol(hz_value, NULL, 10);
    if (!hz_value || system_hz < 2 || system_hz > 5000) system_hz = DEFAULT_HZ;

    watchdog_enabled = 0;
    char* watchdog_value = env_get("watchdog");
    if (watchdog_value) watchdog_enabled = strtol(watchdog_value, NULL, 10);

    if (mb_flags & MULTIBOOT_INFO_VIDEO_INFO) {
    }

    machine.cpu_count = 1;
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

void init_arch()
{
    acpi_init();

#ifdef CONFIG_KVM_GUEST
    kvm_init_guest();
#endif
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
