#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/param.h>
#include <elf.h>

extern char _end[];
extern char** environ;

char* _brksize = NULL;

int syscall_gate_intr(int syscall_nr, MESSAGE* m);

syscall_gate_t _syscall_gate = NULL;

struct sysinfo_user* __lyos_sysinfo = NULL;

static struct sysinfo_user* parse_auxv(char* envp[])
{
    Elf32_auxv_t* auxv;

    if (!envp) return NULL;

    while (*envp++)
        ;

    for (auxv = (Elf32_auxv_t*)envp; auxv->a_type != AT_NULL; auxv++) {
        if (auxv->a_type == AT_SYSINFO) {
            return (struct sysinfo_user*)auxv->a_un.a_val;
        }
    }

    return NULL;
}

static struct sysinfo_user* get_sysinfo()
{
    struct sysinfo_user* sysinfo;
    MESSAGE m;

    m.type = NR_GETINFO;
    m.REQUEST = GETINFO_SYSINFO;
    m.BUF = &sysinfo;

    __asm__ __volatile__("" ::: "memory");

    return (syscall_gate_intr(NR_GETINFO, &m) == 0) ? sysinfo : NULL;
}

void __lyos_init(char* envp[])
{
    environ = envp;
    _syscall_gate = syscall_gate_intr;

    __lyos_sysinfo = parse_auxv(envp);
    if (!__lyos_sysinfo) __lyos_sysinfo = get_sysinfo();

    if (__lyos_sysinfo && __lyos_sysinfo->magic == SYSINFO_MAGIC) {
        _syscall_gate = __lyos_sysinfo->syscall_gate;
    }

    _brksize = (char*)*(&_end);
}
