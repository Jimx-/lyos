#include "type.h"
#include "const.h"

typedef int (*syscall_gate_t)(int syscall_nr, MESSAGE * m);

syscall_gate_t _syscall_gate = (syscall_gate_t)0;

struct sysinfo {
#define SYSINFO_MAGIC   0x534946
    int magic;
    
    syscall_gate_t syscall_gate;
};

int syscall_gate_intr(int syscall_nr, MESSAGE * m)
{
	int a;
	__asm__ __volatile__("int $0x90" : "=a" (a) : "0" (syscall_nr), "b" (m));
	return a;
}

static struct sysinfo * get_sysinfo()
{
	struct sysinfo * sysinfo;
	MESSAGE m;

	m.REQUEST = GETINFO_SYSINFO;
	m.BUF = &sysinfo;
	
	__asm__ __volatile__ ("" ::: "memory");

	return (syscall_gate_intr(NR_GETINFO, &m) == 0) ? sysinfo : (struct sysinfo*)0;
}

void __lyos_init()
{
	_syscall_gate = syscall_gate_intr;

	struct sysinfo * si = get_sysinfo();

	if (si->magic == SYSINFO_MAGIC) {
		_syscall_gate = si->syscall_gate;
	}
}
