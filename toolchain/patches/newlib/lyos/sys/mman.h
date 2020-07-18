#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <sys/types.h>

#define PROT_NONE  0x00 /* no permissions */
#define PROT_READ  0x01 /* pages can be read */
#define PROT_WRITE 0x02 /* pages can be written */
#define PROT_EXEC  0x04 /* pages can be executed */

/*
 * Flags contain sharing type and options.
 * Sharing types; choose one.
 */
#define MAP_SHARED  0x0001 /* share changes */
#define MAP_PRIVATE 0x0002 /* changes are private */

/*
 * Mapping type
 */
#define MAP_ANONYMOUS 0x0004 /* anonymous memory */
#define MAP_ANON      MAP_ANONYMOUS

#define MAP_FIXED    0x0008
#define MAP_POPULATE 0x0010
#define MAP_CONTIG   0x0020

/*
 * Error indicator returned by mmap(2)
 */
#define MAP_FAILED ((void*)-1) /* mmap() failed */

#ifdef __cplusplus
extern "C"
{
#endif

    void* mmap(void* addr, size_t len, int prot, int flags, int fd,
               off_t offset);
    int munmap(void* addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif
