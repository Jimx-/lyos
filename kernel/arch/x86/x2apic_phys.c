#include <lyos/ipc.h>
#include "lyos/const.h"
#include <asm/page.h>
#include <kernel/global.h>
#include "apic.h"

struct apic x2apic_phys = {
    .read = apic_native_msr_read,
    .write = apic_native_msr_write,
    .eoi_write = apic_native_msr_eoi_write,
    .icr_read = x2apic_native_icr_read,
    .icr_write = x2apic_native_icr_write,
};
