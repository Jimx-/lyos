#include <lyos/type.h>
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
#include <lyos/cpulocals.h>
#include <lyos/cpufeature.h>
#include <lyos/spinlock.h>
#include <lyos/time.h>
#include <asm/hpet.h>
#include <asm/div64.h>
#include <asm/type.h>

struct apic apic_flat;

struct apic* apic = &apic_flat;

struct apic apic_flat = {
    .read = apic_native_mem_read,
    .write = apic_native_mem_write,
    .eoi_write = apic_native_mem_eoi_write,
    .icr_read = apic_native_icr_read,
    .icr_write = apic_native_icr_write,
};
