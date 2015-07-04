#include <stdio.h>
#include <elf.h>

static int get_execfd(char* envp[]);

int ldso_start(int argc, char* argv[], char* envp[])
{
	_exit(0);
	write(1,"Hello world\n", 11);
	/* get executable fd */
	int execfd = get_execfd(envp);
	if (execfd < 0) return 1;

	return 0;
}

static int get_execfd(char* envp[])
{
	Elf32_auxv_t *auxv;
    while(*envp++ != NULL);

    for (auxv = (Elf32_auxv_t *)envp; auxv->a_type != AT_NULL; auxv++)
    {
        if( auxv->a_type == AT_EXECFD) return auxv->a_un.a_val;
    }

    return -1;
}
