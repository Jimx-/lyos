#include "type.h"
#include "ipc.h"
#include "const.h"

#include <elf.h>

extern char _end[];

char * _brksize = (char *)0;

typedef int (*syscall_gate_t)(int syscall_nr, MESSAGE * m);

int syscall_gate_intr(int syscall_nr, MESSAGE * m);

syscall_gate_t _syscall_gate = (syscall_gate_t)0;

struct sysinfo {
#define SYSINFO_MAGIC   0x534946
    int magic;
    
    syscall_gate_t syscall_gate;
};

static struct sysinfo* parse_auxv(char* envp[])
{
	Elf32_auxv_t *auxv;

	if (!envp) return (struct sysinfo*) 0;

    while(*envp++);

    for (auxv = (Elf32_auxv_t *)envp; auxv->a_type != AT_NULL; auxv++)
    {
    	if (auxv->a_type == AT_SYSINFO) {
    		return (struct sysinfo*) auxv->a_un.a_val;
    	}
    }

    return (struct sysinfo*) 0;
}

static struct sysinfo * get_sysinfo()
{
	struct sysinfo * sysinfo;
	MESSAGE m;

	m.type = NR_GETINFO;
	m.REQUEST = GETINFO_SYSINFO;
	m.BUF = &sysinfo;
	
	__asm__ __volatile__ ("" ::: "memory");

	return (syscall_gate_intr(NR_GETINFO, &m) == 0) ? sysinfo : (struct sysinfo*)0;
}

void __lyos_init(char* envp[])
{
	_syscall_gate = syscall_gate_intr;

	struct sysinfo * si = parse_auxv(envp);
	if (!si) si = get_sysinfo();

	if (si && si->magic == SYSINFO_MAGIC) {
		_syscall_gate = si->syscall_gate;
	}

	_brksize = (char *)*(&_end);
}
