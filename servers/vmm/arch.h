#ifndef _VMM_ARCH_H_
#define _VMM_ARCH_H_

#include <lyos/hypervisor.h>
#include <lyos/vmm.h>

struct vmm_vm;

/* Architecture policy has no IPC responsibilities. The generic service owns
 * client authorization, VM/RAM lifetime, and copying requests and replies.
 */
struct vmm_arch_ops {
    u32 backend;
    int (*init_guest)(struct vmm_vm* vm, u64 entry);
    int (*run)(struct vmm_vm* vm, struct vmm_run_status* status);
};

extern const struct vmm_arch_ops vmm_arch_ops;

#endif
