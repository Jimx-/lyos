#ifndef _LIBEXEC_H_
#define _LIBEXEC_H_

struct exec_info;

typedef int (*libexec_exec_loadfunc_t)(struct exec_info *execi);

struct exec_info {
	endpoint_t proc_e;

	char * header;
	size_t header_len;

	char prog_name[MAX_PATH];
	size_t filesize;
	
	size_t stack_size;

	int stack_top;
};

PUBLIC int libexec_load_elf(struct exec_info * execi);

#endif /* _LIBEXEC_H_ */
