#include <lyos/ipc.h>
#include "lyos/const.h"
#include <asm/page.h>
#include <kernel/global.h>
#include "apic.h"

struct apic apic_flat;

struct apic* apic = &apic_flat;

struct apic apic_flat = {
    .read = apic_native_mem_read,
    .write = apic_native_mem_write,
    .eoi_write = apic_native_mem_eoi_write,
    .icr_read = apic_native_icr_read,
    .icr_write = apic_native_icr_write,
};
