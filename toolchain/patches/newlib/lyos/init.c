#include "const.h"

typedef int (*syscall_gate_t)(int syscall_nr, int arg0, int arg1, int arg2);

syscall_gate_t _syscall_gate;

struct sysinfo {
#define SYSINFO_MAGIC   0x534946
    int magic;
    
    int (*syscall_gate)(int syscall_nr, int arg0, int arg1, int arg2);
};

int syscall_gate_intr(int syscall_nr, int arg0, int arg1, int arg2)
{
	int a;
	__asm__ __volatile__("int $0x90" : "=a" (a) : "0" (syscall_nr), "b" (arg0), "c" (arg1), "d" (arg2));
	return a;
}

static struct sysinfo * get_sysinfo()
{
	int a;
	struct sysinfo * sysinfo;
	__asm__ __volatile__("int $0x90" : "=a" (a) : "0" (4), "c" (GETINFO_SYSINFO), "d" (&sysinfo));
	return a == 0 ? sysinfo : (struct sysinfo*)0;
}

void __lyos_init()
{
	_syscall_gate = syscall_gate_intr;

	struct sysinfo * si = get_sysinfo();

	if (si->magic == SYSINFO_MAGIC) {
		_syscall_gate = si->syscall_gate;
	}
}
