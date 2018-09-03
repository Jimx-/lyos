#ifndef _LIBEXEC_H_
#define _LIBEXEC_H_

struct exec_info;

typedef int (*libexec_exec_loadfunc_t)(struct exec_info *execi);

typedef int (*libexec_allocator_t)(struct exec_info *execi, void* vaddr, size_t len);
typedef int (*libexec_clearmem_t)(struct exec_info *execi, void* vaddr, size_t len);
typedef int (*libexec_clearproc_t)(struct exec_info *execi);
typedef int (*libexec_copymem_t)(struct exec_info *execi, off_t offset, void* vaddr, size_t len);
typedef int (*libexec_mmap_t)(struct exec_info *execi, void* vaddr, size_t len, off_t foffset, int protflags, size_t clearend);

struct exec_info {
	endpoint_t proc_e;

	char* header;
	size_t header_len;

	char prog_name[MAX_PATH];
	size_t filesize;

	libexec_allocator_t allocmem;
	libexec_allocator_t allocmem_prealloc;
	libexec_copymem_t copymem;
	libexec_clearmem_t clearmem;
	libexec_clearproc_t clearproc;
	libexec_mmap_t memmap;

	void* callback_data;

	size_t text_size;
	size_t data_size;
	size_t stack_size;

	uid_t new_uid, new_euid;
	gid_t new_gid, new_egid;

	void* entry_point;
	void* stack_top;
	void* brk;

	void* phdr;
	unsigned int phnum;
	void* load_base;
};

PUBLIC int libexec_allocmem(struct exec_info * execi, void* vaddr, size_t len);
PUBLIC int libexec_allocmem_prealloc(struct exec_info * execi, void* vaddr, size_t len);
PUBLIC int libexec_clearproc(struct exec_info * execi);
PUBLIC int libexec_clearmem(struct exec_info * execi, void* vaddr, size_t len);

PUBLIC int libexec_load_elf(struct exec_info * execi);
PUBLIC int elf_is_dynamic(char* hdr, size_t hdr_len, char* interp, size_t maxlen);

#endif /* _LIBEXEC_H_ */
