#include <stdio.h>
#include <elf.h>
#include <unistd.h>
#include <string.h>

#include "ldso.h"
#include "env.h"

#define NR_SO_INFO 64
struct so_info si_pool[NR_SO_INFO];
struct so_info si_self;
struct so_info *si_list = NULL, *si_list_tail;

struct search_paths ld_paths;
struct search_paths ld_default_paths;

extern char _DYNAMIC;
extern char _GLOBAL_OFFSET_TABLE_;

int* __errno(void)
{
    static int _e = 0;
    return &_e;
}

void ldso_die(char* reason)
{
    xprintf("Fatal error: %s\n", reason);
    _exit(1);
}

static void init_si_pool()
{
    int i;
    for (i = 0; i < NR_SO_INFO; i++) {
        si_pool[i].flags = 0;
    }
}

struct so_info* ldso_alloc_info(const char* name)
{
    int i;
    struct so_info* si = NULL;
    int name_len = strlen(name);

    if (name_len >= sizeof(si->name)) return NULL;

    for (i = 0; i < NR_SO_INFO; i++) {
        if (si_pool[i].flags == 0) break;
    }

    if (i == NR_SO_INFO) return NULL;
    si = &si_pool[i];

    memcpy((char*)si->name, name, name_len);
    si->name[name_len] = '\0';
    si->name_len = name_len;
    si->flags = SI_USED;
    si->next = NULL;
    si->list = NULL;

    if (si_list == NULL) {
        si_list_tail = si_list = si;
    } else {
        si_list_tail->next = si;
        si_list_tail = si;
    }

    return si;
}

static void ldso_init_self(char* interp_base, char* relocbase)
{
    char* self_name = LDSO_PATH;

    memcpy((char*)si_self.name, self_name, strlen(self_name));

    si_self.mapbase = interp_base;
    si_self.relocbase = relocbase;
    si_self.dynamic = (Elf32_Dyn*)&_DYNAMIC;
    si_self.next = NULL;

    ldso_process_dynamic(&si_self);

    ldso_init_paths(&ld_default_paths);
    ldso_init_paths(&ld_paths);
    ldso_add_paths(&ld_default_paths, DEFAULT_LD_PATHS);
}

struct so_info* ldso_get_obj_from_addr(const void* addr)
{
    struct so_info* si;

    for (si = si_list; si != NULL; si = si->next) {
        if (addr < (void*)si->mapbase) continue;
        if (addr < (void*)(si->mapbase + si->mapsize)) return si;
    }
    return NULL;
}

struct so_info* ldso_check_handle(void* handle)
{
    struct so_info* si;

    for (si = si_list; si != NULL; si = si->next) {
        if (handle == (void*)si) break;
    }
    return si;
}

int ldso_main(int argc, char* argv[], char* envp[])
{
    Elf32_Addr got0;

    init_si_pool();
    /* parse environments and aux vectors */
    setenv(envp);

    const char* ld_library_path = env_get("LD_LIBRARY_PATH");
    int show_auxv = 0;
    const char* show_auxv_env = env_get("LD_SHOW_AUXV");
    if (show_auxv_env) {
        show_auxv = show_auxv_env[0] - '0';
    }

    int bind_now = 0;
    const char* bind_now_env = env_get("LD_BIND_NOW");
    if (bind_now_env) {
        bind_now = bind_now_env[0] - '0';
    }

    struct so_info* si = ldso_alloc_info("Main executable");
    if (si == NULL) return -1;
    si->flags |= SI_EXEC;

    char* interp_base;
    parse_auxv(envp, si, show_auxv, &interp_base);
    if (interp_base == (char*)-1) ldso_die("AT_BASE not set");

    got0 = *(Elf32_Addr*)&_GLOBAL_OFFSET_TABLE_;
    ldso_init_self(interp_base, &_DYNAMIC - got0);

    if (ld_library_path) ldso_add_paths(&ld_paths, ld_library_path);

    ldso_process_phdr(si, si->phdr, si->phnum);
    ldso_process_dynamic(si);

    ldso_load_needed(si);

    ldso_relocate_objects(si, bind_now);

    ldso_do_copy_relocations(si);

    return (int)si->entry;
}
