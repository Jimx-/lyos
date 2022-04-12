#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include <asm/protect.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "acpi.h"
#include "apic.h"
#include <asm/proto.h>
#include <asm/const.h>
#ifdef CONFIG_SMP
#include <asm/smp.h>
#endif
#include <asm/hwint.h>
#include <asm/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/spinlock.h>
#include <lyos/time.h>
#include <asm/hpet.h>
#include <asm/div64.h>

struct apic x2apic_phys = {
    .read = apic_native_msr_read,
    .write = apic_native_msr_write,
    .eoi_write = apic_native_msr_eoi_write,
    .icr_read = x2apic_native_icr_read,
    .icr_write = x2apic_native_icr_write,
};
