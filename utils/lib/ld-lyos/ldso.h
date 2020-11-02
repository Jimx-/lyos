#ifndef _LDSO_H_
#define _LDSO_H_

#include <sys/types.h>
#include <sys/tls.h>
#include <sys/syslimits.h>
#include <elf.h>
#include "list.h"

#define LDSO_PATH        "/lib/ld-lyos.so"
#define DEFAULT_LD_PATHS "/usr/lib:/lib"

#define SI_USED  1
#define SI_EXEC  2
#define SI_SHLIB 4

#define LDSO_PUBLIC __attribute__((__visibility__("default")))

struct needed_entry;

typedef void (*so_func_t)(void);

struct so_info {
    int flags;
    char name[PATH_MAX];
    int name_len;
    int refcnt;

    int dev;
    int ino;

    Elf32_Ehdr* ehdr;
    Elf32_Phdr* phdr;
    int phnum;
    char* entry;
    int is_dynamic;

    char* mapbase;
    size_t mapsize;
    char* relocbase;
    Elf32_Dyn* dynamic;

    Elf32_Rel *rel, *relend;
    Elf32_Rel *pltrel, *pltrelend;
    Elf32_Sym* symtab;
    char* strtab;
    int strtabsz;
    Elf32_Addr* pltgot;

    unsigned nbuckets;
    int* buckets;
    unsigned nchains;
    int* chains;

    size_t tls_index;
    void* tls_init;
    size_t tls_init_size;
    size_t tls_size;
    size_t tls_offset;
    size_t tls_align;
    int tls_done;

    so_func_t init;
    so_func_t fini;
    int init_done;
    int init_called;

    Elf32_Addr* init_array;
    size_t init_array_size;
    Elf32_Addr* fini_array;
    size_t fini_array_size;

    struct needed_entry* needed;

    struct list_head list;
    struct so_info* next;
};

extern struct so_info si_self;

extern size_t ldso_tls_dtv_generation;
extern size_t ldso_tls_max_index;

#define MAX_SEARCH_PATHS 16
struct search_path {
    char pathname[PATH_MAX];
    int name_len;
};

struct search_paths {
    struct search_path paths[MAX_SEARCH_PATHS];
    int count;
};

struct needed_entry {
    unsigned long name;
    struct so_info* si;
    struct needed_entry* next;
};

extern struct search_paths ld_paths;
extern struct search_paths ld_default_paths;
extern size_t pagesz;
extern struct so_info* si_list;

#define roundup(x) \
    (((x) % pagesz == 0) ? (x) : (((x) + pagesz) - ((x) % pagesz)))
#define rounddown(x) ((x) - ((x) % pagesz))

int ldso_process_dynamic(struct so_info* si);
int ldso_process_phdr(struct so_info* si, Elf32_Phdr* phdr, int phnum);
struct so_info* ldso_alloc_info(const char* name);
void ldso_die(char* reason);
struct so_info* ldso_get_obj_from_addr(const void* addr);
struct so_info* ldso_check_handle(void* handle);
void ldso_setup_pltgot(struct so_info* si);
int ldso_relocate_plt_lazy(struct so_info* si);
int ldso_relocate_nonplt_objects(struct so_info* si);
int ldso_do_copy_relocations(struct so_info* si);
Elf32_Sym* ldso_find_plt_sym(struct so_info* si, unsigned long symnum,
                             struct so_info** obj);
struct so_info* ldso_map_object(const char* pathname, int fd);
unsigned long ldso_elf_hash(const char* name);
Elf32_Sym* ldso_lookup_symbol_obj(const char* name, unsigned long hash,
                                  struct so_info* si, int in_plt);
Elf32_Sym* ldso_find_sym(struct so_info* si, unsigned long symnum,
                         struct so_info** obj, int in_plt);
struct so_info* ldso_load_library(const char* name, struct so_info* who);
int ldso_load_needed(struct so_info* first);
int ldso_relocate_objects(struct so_info* first, int bind_now);
void ldso_init_paths(struct search_paths* list);
void ldso_add_paths(struct search_paths* list, const char* paths);

LDSO_PUBLIC void* dlopen(const char* filename, int flags);
LDSO_PUBLIC void* dlsym(void* handle, const char* name);

#if defined(__HAVE_TLS_VARIANT_1) || defined(__HAVE_TLS_VARIANT_2)
#define LDSO_STATIC_TLS_RESERVATION 64

LDSO_PUBLIC struct tls_tcb* __ldso_allocate_tls(void);

void ldso_tls_initial_allocation(void);
int ldso_tls_allocate_offset(struct so_info* si);
#endif

int xprintf(const char* fmt, ...);

#endif
