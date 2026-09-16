#include <kernel/hypervisor.h>

#ifdef CONFIG_INTEL_VMX
#include "vmx/vmx.h"
#endif
#ifdef CONFIG_AMD_SVM
#include "svm/svm.h"
#endif

void arch_hv_init(void)
{
#ifdef CONFIG_INTEL_VMX
    vmx_early_init();
#endif
#ifdef CONFIG_AMD_SVM
    svm_early_init();
#endif
}
