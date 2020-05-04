#ifndef _KVM_PARA_H_
#define _KVM_PARA_H_

#include <asm/kvm_para.h>

static inline int kvm_para_has_feature(unsigned int feature)
{
    return !!(kvm_arch_para_features() & (1UL << feature));
}

#endif
