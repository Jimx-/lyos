#include <kernel/hypervisor.h>

#ifdef CONFIG_INTEL_VMX
#include "vmx/vmx.h"
#endif

void arch_hv_init(void)
{
#ifdef CONFIG_INTEL_VMX
    vmx_early_init();
#endif
}
