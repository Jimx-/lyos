#ifndef _LYOS_ACPI_UTILS_H_
#define _LYOS_ACPI_UTILS_H_

#include <sys/types.h>
#include <lyos/types.h>

#define ACPI_NAME_LEN 4

#define ACPI_SIG(a, b, c, d) \
    ((u32)(a) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

#define ACPI_SIG_MCFG ACPI_SIG('M', 'C', 'F', 'G')

struct acpi_table_info {
    u32 signature;
    u32 index;
    phys_bytes phys_addr;
    size_t length;
    u8 revision;
};

struct acpi_mcfg_entry {
    u64 base_addr;
    u16 segment;
    u8 start_bus;
    u8 end_bus;
    u32 reserved;
};

int acpi_get_table_info(u32 signature, u32 index, struct acpi_table_info* info);
int acpi_copy_table(u32 signature, u32 index, void* buf, size_t len);
int acpi_get_mcfg_count(void);
int acpi_get_mcfg_entry(u32 index, struct acpi_mcfg_entry* entry);

#endif
