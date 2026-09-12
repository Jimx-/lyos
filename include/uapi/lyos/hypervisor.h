#ifndef _UAPI_LYOS_HYPERVISOR_H_
#define _UAPI_LYOS_HYPERVISOR_H_

#include <lyos/types.h>

/* Architecture-neutral kernel hypervisor control protocol. */
#define HV_ABI_VERSION 1

#define HVCTL_QUERY_CAPS         0
#define HVCTL_VM_CREATE          1
#define HVCTL_VM_DESTROY         2
#define HVCTL_VM_SET_MEMSLOT     3
#define HVCTL_VCPU_CREATE        4
#define HVCTL_VCPU_DESTROY       5
#define HVCTL_VCPU_SET_STATE     6
#define HVCTL_VCPU_GET_STATE     7
#define HVCTL_VCPU_RUN           8
#define HVCTL_REGISTER_BACKING   9
#define HVCTL_GET_RELEASE        10
#define HVCTL_UNREGISTER_BACKING 11
#define HVCTL_CHECK_CLIENT       12

#define HV_CAP_STAGE2 0x01
#define HV_CAP_X86_32 0x02
#define HV_CAP_ACTIVE 0x08

#define HV_BACKEND_NONE 0
#define HV_BACKEND_VMX  1

struct hv_caps {
    __u32 abi_version;
    __u32 caps;
    __u32 active_vms;
    __u32 active_vcpus;
    __u32 translation_pages;
    __u32 backings;
    __u32 backing_pages;
    __u32 backend;
};

#define HV_MEM_READ  0x1
#define HV_MEM_WRITE 0x2
#define HV_MEM_EXEC  0x4
#define HV_MEM_ALL   (HV_MEM_READ | HV_MEM_WRITE | HV_MEM_EXEC)

struct hv_memslot {
    __u64 gpa;
    __u64 len;
    __u64 offset;
    __u32 mm_handle;
    __u32 flags;
    __u32 reserved[2];
};

/* State and exit records are defined by the selected backend ABI. */
int hv_query_caps(struct hv_caps* caps);
int hv_vm_create(u32* handle);
int hv_vm_destroy(u32 handle);
int hv_vm_set_memslot(u32 vm, const struct hv_memslot* slot);
int hv_vcpu_create(u32 vm, u32* vcpu);
int hv_vcpu_destroy(u32 vcpu);
int hv_vcpu_set_state(u32 vcpu, const void* state, size_t size);
int hv_vcpu_get_state(u32 vcpu, void* state, size_t size);
int hv_vcpu_run(u32 vcpu, void* exit, size_t size);
/* Returns ESRCH once the full client endpoint is no longer live. */
int hv_check_client(endpoint_t client);

/* MM-only registration operations. */
int hv_register_backing(u32 mm_handle, unsigned long npages, u64* pages);
int hv_get_release(u32* mm_handle);
int hv_unregister_backing(u32 mm_handle);
int mm_guest_ram_alloc(size_t length, u32* handle, void** vaddr);
int mm_guest_ram_free(u32 handle);

#endif
