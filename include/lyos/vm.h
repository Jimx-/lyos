/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _VM_H_
#define _VM_H_

#include <sys/types.h>
#include <lyos/types.h>

#define MMSUSPEND -1001

#define PHYS_NONE ((phys_bytes)-2)

#define PCTL_WHO   u.m3.m3i1
#define PCTL_PARAM u.m3.m3i2

#define PCTL_CLEARPROC 1
#define PCTL_CLEARMEM  2

/* vmctl request */
#define VMCTL_BOOTINHIBIT_CLEAR  1
#define VMCTL_MMINHIBIT_CLEAR    2
#define VMCTL_GET_KERN_MAPPING   3
#define VMCTL_REPLY_KERN_MAPPING 4
#define VMCTL_PAGEFAULT_CLEAR    5
#define VMCTL_GETPDBR            6
#define VMCTL_SET_ADDRESS_SPACE  7
#define VMCTL_CLEAR_MEMCACHE     8
#define VMCTL_GET_MMREQ          9
#define VMCTL_REPLY_MMREQ        10
#define VMCTL_FLUSHTLB           11
#ifdef __aarch64__
#define VMCTL_GETKPDBR       12
#define VMCTL_SET_KADDRSPACE 13
#endif

#define VMCTL_GET_KM_INDEX  u.m3.m3i2
#define VMCTL_GET_KM_RETVAL u.m3.m3i2
#define VMCTL_GET_KM_ADDR   u.m3.m3p1
#define VMCTL_GET_KM_LEN    u.m3.m3i3
#define VMCTL_GET_KM_FLAGS  u.m3.m3i4

#define VMCTL_REPLY_KM_INDEX  u.m3.m3i2
#define VMCTL_REPLY_KM_RETVAL u.m3.m3i2
#define VMCTL_REPLY_KM_ADDR   u.m3.m3p1

#define VMCTL_REQUEST   u.m3.m3i1
#define VMCTL_WHO       u.m3.m3i2
#define VMCTL_VALUE     u.m3.m3i3
#define VMCTL_PHYS_ADDR u.m3.m3l1
#define VMCTL_VIR_ADDR  u.m3.m3p1

#define VMCTL_MMREQ_TARGET    u.m3.m3i1
#define VMCTL_MMREQ_ADDR      u.m3.m3p1
#define VMCTL_MMREQ_LEN       u.m3.m3i2
#define VMCTL_MMREQ_FLAGS     u.m3.m3i3
#define VMCTL_MMREQ_CALLER    u.m3.m3i4
#define MMREQ_TYPE_SYSCALL    1
#define MMREQ_TYPE_DELIVERMSG 2
#define MMREQ_CHECK           1

/* umap() request */
#define UMAP_WHO     u.m3.m3i1
#define UMAP_TYPE    u.m3.m3i2
#define UMAP_SIZE    u.m3.m3l1
#define UMAP_SRCADDR u.m3.m3p1
#define UMAP_DSTADDR u.m3.m3p2

#define UMT_GRANT 1
#define UMT_VADDR 2

#define KMF_WRITE 0x01
#define KMF_USER  0x02
#define KMF_EXEC  0x04
#define KMF_IO    0x08

#define MM_GET_MEMINFO 1

/* Fault flags */
#define FAULT_FLAG_WRITE       0x1
#define FAULT_FLAG_USER        0x2
#define FAULT_FLAG_INSTRUCTION 0x4

struct mem_info {
    size_t mem_total;
    size_t mem_free;
    size_t cached;
    size_t slab;
    size_t vmalloc_total;
    size_t vmalloc_used;
};

struct mm_fork_info {
    endpoint_t parent;
    int slot;
    int flags;
    void* newsp;
    void* tls;
};

struct mm_map_phys_request {
    endpoint_t who;
    phys_bytes phys_addr;
    size_t len;
    int flags;
#define MMP_IO 0x01
};

int vmctl(int request, endpoint_t who);
int procctl(endpoint_t who, int param);
int vmctl_get_kern_mapping(int index, caddr_t* addr, int* len, int* flags);
int vmctl_reply_kern_mapping(int index, void* vir_addr);
int vmctl_getpdbr(endpoint_t who, unsigned long* pdbr);
int vmctl_set_address_space(endpoint_t who, unsigned long pgd_phys);
int umap(endpoint_t ep, int type, vir_bytes vir_addr, vir_bytes size,
         phys_bytes* phys_addr);
void* mm_map_phys(endpoint_t who, phys_bytes phys_addr, size_t len, int flags);
int vmctl_get_mmrequest(endpoint_t* target, void** start, size_t* len,
                        int* flags, endpoint_t* caller);
int vmctl_reply_mmreq(endpoint_t who, int result);
int vmctl_flushtlb(endpoint_t who);
int get_meminfo(struct mem_info* mem_info);
int vfs_mmap(endpoint_t who, off_t offset, size_t len, dev_t dev, ino_t ino,
             int fd, void* vaddr, int flags, int prot, size_t clearend);

#ifdef __aarch64__
int vmctl_getkpdbr(endpoint_t who, unsigned long* kpdbr);
int vmctl_set_kern_address_space(unsigned long pgd_phys);
#endif

#endif
