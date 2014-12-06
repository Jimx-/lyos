#ifndef _LIBEXEC_H_
#define _LIBEXEC_H_

struct exec_info;

typedef int (*libexec_exec_loadfunc_t)(struct exec_info *execi);

typedef int (*libexec_allocator_t)(struct exec_info *execi, int vaddr, size_t len);
typedef int (*libexec_clearmem_t)(struct exec_info *execi, int vaddr, size_t len);
typedef int (*libexec_clearproc_t)(struct exec_info *execi);
typedef int (*libexec_copymem_t)(struct exec_info *execi, off_t offset, int vaddr, size_t len);
typedef int (*libexec_mmap_t)(struct exec_info *execi,
	int vaddr, int len, int foffset, int protflags);

struct exec_info {
	endpoint_t proc_e;

	char * header;
	size_t header_len;

	char prog_name[MAX_PATH];
	size_t filesize;
	
	libexec_allocator_t allocmem;
	libexec_allocator_t alloctext;
	libexec_allocator_t allocstack;
	libexec_copymem_t copymem;
	libexec_clearmem_t clearmem;
	libexec_clearproc_t clearproc;
	libexec_mmap_t memmap;

	void * callback_data;

	size_t text_size;
	size_t data_size;
	size_t stack_size;

	int entry_point;
	int stack_top;
	int brk;
};

PUBLIC int libexec_allocmem(struct exec_info * execi, int vaddr, size_t len);
PUBLIC int libexec_alloctext(struct exec_info * execi, int vaddr, size_t len);
PUBLIC int libexec_allocstack(struct exec_info * execi, int vaddr, size_t len);
PUBLIC int libexec_clearproc(struct exec_info * execi);
PUBLIC int libexec_clearmem(struct exec_info * execi, int vaddr, size_t len);

PUBLIC int libexec_load_elf(struct exec_info * execi);

#endif /* _LIBEXEC_H_ */
