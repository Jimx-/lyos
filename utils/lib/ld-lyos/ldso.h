#ifndef _LDSO_H_
#define _LDSO_H_

#include <sys/types.h>
#include <sys/syslimits.h>

#define LDSO_PATH 			"/lib/ld-lyos.so"
#define DEFAULT_LD_PATHS	"/usr/lib"

#define SI_USED		1
#define SI_EXEC     2
#define SI_SHLIB    4

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
    char* relocbase;
    Elf32_Dyn* dynamic;

    Elf32_Rel* rel, *relend;
    Elf32_Rel* pltrel, *pltrelend;
    Elf32_Sym* symtab;
    char* strtab;
    int strtabsz;
    Elf32_Addr* pltgot;

    unsigned nbuckets;
    int* buckets;
    unsigned nchains;
    int* chains;

    so_func_t init;
    so_func_t fini;

    struct needed_entry* needed;

    struct so_info* list;
    struct so_info* next;
};

#define MAX_SEARCH_PATHS	16
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

int ldso_process_dynamic(struct so_info* si);
int ldso_process_phdr(struct so_info* si, Elf32_Phdr* phdr, int phnum);
struct so_info* alloc_info(const char* name);
void ldso_die(char* reason);
void ldso_setup_pltgot(struct so_info* si);
int ldso_relocate_plt_lazy(struct so_info* si);
Elf32_Sym* ldso_find_plt_sym(struct so_info* si, unsigned long symnum, struct so_info** obj);

#endif

