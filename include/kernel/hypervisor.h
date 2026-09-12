#ifndef _KERNEL_HYPERVISOR_H_
#define _KERNEL_HYPERVISOR_H_

#include <lyos/hypervisor.h>

struct proc;

/* The control layer copies bounded opaque records; only the backend interprets
 * their architecture-specific contents. Backends own VM/vCPU handles and must
 * enforce endpoint ownership on every operation.
 */
#define HV_STATE_MAX_SIZE     4096
#define HV_EXIT_MAX_SIZE      256
#define HV_MAX_BACKINGS       16
#define HV_BACKING_POOL_PAGES 16384
#define HV_RELEASE_QUEUE      64

struct hv_backend_ops {
    u32 backend;
    size_t state_size, exit_size;
    int (*query_caps)(struct hv_caps* caps);
    int (*vm_create)(endpoint_t owner, u32* handle);
    int (*vm_destroy)(endpoint_t owner, u32 handle);
    int (*vm_set_memslot)(endpoint_t owner, u32 handle,
                          const struct hv_memslot* slot);
    int (*vcpu_create)(endpoint_t owner, u32 vm, u32* vcpu);
    int (*vcpu_destroy)(endpoint_t owner, u32 vcpu);
    int (*vcpu_set_state)(endpoint_t owner, u32 vcpu, const void* state);
    int (*vcpu_get_state)(endpoint_t owner, u32 vcpu, void* state);
    int (*vcpu_run)(endpoint_t owner, u32 vcpu, void* exit);
    void (*proc_cleanup)(struct proc* p);
};

struct hv_backing {
    int in_use;
    u32 mm_handle;
    unsigned long npages, pool_off;
    unsigned int refs;
};

#ifdef CONFIG_HYPERVISOR
extern const struct hv_backend_ops* hv_backend;
int hv_backend_register(const struct hv_backend_ops* ops);
void hv_init(void);
void arch_hv_init(void);
void hv_proc_cleanup(struct proc* p);

int hv_backing_register(u32 mm_handle, unsigned long npages, const u64* pages);
int hv_backing_unregister(u32 mm_handle);
int hv_backing_release_get(u32* mm_handle);
struct hv_backing* hv_backing_lookup(u32 mm_handle);
void hv_backing_get(struct hv_backing* backing);
void hv_backing_put(struct hv_backing* backing);
u64 hv_backing_page(const struct hv_backing* backing, unsigned long index);
void hv_backing_query_caps(struct hv_caps* caps);
#else
static inline void hv_init(void) {}
static inline void hv_proc_cleanup(struct proc* p) {}
#endif

#endif
