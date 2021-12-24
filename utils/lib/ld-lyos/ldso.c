#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>

#include "ldso.h"
#include "env.h"

#define NR_SO_INFO 64
struct so_info si_pool[NR_SO_INFO];
struct so_info si_self;
struct so_info *si_list = NULL, *si_list_tail;

struct search_paths ld_paths;
struct search_paths ld_default_paths;

extern __attribute__((visibility("hidden"))) char _DYNAMIC;
extern __attribute__((visibility("hidden"))) char _GLOBAL_OFFSET_TABLE_;

char** environ;

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

    memset(si, 0, sizeof(*si));
    memcpy((char*)si->name, name, name_len);
    si->name[name_len] = '\0';
    si->name_len = name_len;
    si->flags = SI_USED;

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
    si_self.dynamic = (ElfW(Dyn)*)&_DYNAMIC;
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

static void ldso_init_tpsort_visit(struct list_head* head, struct so_info* si,
                                   int rev)
{
    struct needed_entry* entry;

    if (si->init_done) return;
    si->init_done = 1;

    for (entry = si->needed; entry; entry = entry->next) {
        if (entry->si) ldso_init_tpsort_visit(head, entry->si, rev);
    }

    if (rev)
        list_add(&si->list, head);
    else
        list_add_tail(&si->list, head);
}

static void ldso_init_tpsort(struct list_head* head, int rev)
{
    struct so_info* si;

    for (si = si_list; si; si = si->next) {
        si->init_done = 0;
    }

    for (si = si_list; si; si = si->next) {
        ldso_init_tpsort_visit(head, si, rev);
    }
}

static void ldso_call_initfini_function(ElfW(Addr) func)
{
    ((void (*)(void))(uintptr_t)func)();
}

static void ldso_call_init_function(struct so_info* si)
{
    if (si->init_array_size == 0 && (si->init_called || !si->init)) return;

    if (!si->init_called && si->init) {
        si->init_called = 1;
        si->init();
    }

    while (si->init_array_size > 0) {
        ElfW(Addr) init = *si->init_array++;
        si->init_array_size--;
        ldso_call_initfini_function(init);
    }
}

static void ldso_call_init_functions(void)
{
    struct list_head init_list;
    struct so_info* si;

    INIT_LIST_HEAD(&init_list);
    ldso_init_tpsort(&init_list, 0);

    list_for_each_entry(si, &init_list, list) { ldso_call_init_function(si); }
}

void* ldso_main(int argc, char* argv[], char* envp[])
{
    ElfW(Addr) got0;
    char* got_addr;

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
    if (si == NULL) return (void*)-1;
    si->flags |= SI_EXEC;

    char* interp_base;
    parse_auxv(envp, si, show_auxv, &interp_base);
    if (interp_base == (char*)-1ULL) ldso_die("AT_BASE not set");

    got0 = *(ElfW(Addr)*)&_GLOBAL_OFFSET_TABLE_;
    ldso_init_self(interp_base, &_DYNAMIC - got0);

    if (ld_library_path) ldso_add_paths(&ld_paths, ld_library_path);

    ldso_process_phdr(si, si->phdr, si->phnum);
    ldso_process_dynamic(si);

    ldso_load_needed(si);

#if defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2)
    struct so_info* si_tls;

    for (si_tls = si_list; si_tls != NULL; si_tls = si_tls->next) {
        ldso_tls_allocate_offset(si);
    }
#endif

    ldso_relocate_objects(si, bind_now);

    ldso_do_copy_relocations(si);

#if defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2)
    ldso_tls_initial_allocation();
#endif

    ldso_call_init_functions();

    return si->entry;
}

void* dlopen(const char* filename, int flags)
{
    struct so_info* si;
    struct so_info** old_tail = &si_list_tail->next;
    int retval;

    if (flags & RTLD_NOLOAD) return NULL;

    if (filename == NULL) {
        si = si_list;
    } else {
        si = ldso_load_library(filename, si_list);
    }

    if (si) {
        if (*old_tail) {
            retval = ldso_load_needed(si);

            if (retval == -1 ||
                ldso_relocate_objects(si, flags & RTLD_NOW) == -1) {
                si = NULL;
            } else {
                ldso_call_init_functions();
            }
        }
    }

    return si;
}
