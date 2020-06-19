#ifndef _X86_ACPI_H_
#define _X86_ACPI_H_

/* ACPI root system description pointer */
struct acpi_rsdp {
    char signature[8]; /* must be "RSD PTR " */
    u8 checksum;
    char oemid[6];
    u8 revision;
    u32 rsdt_addr;
    u32 length;
};

#define ACPI_SDT_SIGNATURE_LEN 4

#define ACPI_SDT_SIGNATURE(name) #name

/* header common to all system description tables */
struct acpi_sdt_header {
    char signature[ACPI_SDT_SIGNATURE_LEN];
    u32 length;
    u8 revision;
    u8 checksum;
    char oemid[6];
    char oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
};

struct acpi_generic_address {
    u8 address_space_id;
    u8 register_bit_width;
    u8 register_bit_offset;
    u8 access_size;
    u64 address;
};

struct acpi_madt_hdr {
    struct acpi_sdt_header hdr;
    u32 local_apic_address;
    u32 flags;
};

#define ACPI_MADT_TYPE_LAPIC 0
#define ACPI_MADT_TYPE_IOAPIC 1
#define ACPI_MADT_TYPE_INT_SRC 2
#define ACPI_MADT_TYPE_NMI_SRC 3
#define ACPI_MADT_TYPE_LAPIC_NMI 4
#define ACPI_MADT_TYPE_LAPIC_ADRESS 5
#define ACPI_MADT_TYPE_IOSAPIC 6
#define ACPI_MADT_TYPE_LSAPIC 7
#define ACPI_MADT_TYPE_PLATFORM_INT_SRC 8
#define ACPI_MADT_TYPE_Lx2APIC 9
#define ACPI_MADT_TYPE_Lx2APIC_NMI 10

struct acpi_madt_item_hdr {
    u8 type;
    u8 length;
};

struct acpi_madt_lapic {
    struct acpi_madt_item_hdr hdr;
    u8 acpi_cpu_id;
    u8 apic_id;
    u32 flags;
};

struct acpi_madt_ioapic {
    struct acpi_madt_item_hdr hdr;
    u8 id;
    u8 __reserved;
    u32 address;
    u32 global_int_base;
};

struct acpi_madt_int_src {
    struct acpi_madt_item_hdr hdr;
    u8 bus;
    u8 bus_int;
    u32 global_int;
    u16 mps_flags;
};

struct acpi_hpet {
    struct acpi_sdt_header hdr;
    u32 block_id;
    struct acpi_generic_address address;
    u8 hpet_number;
    u16 minimum_tick;
    u8 page_protection;
};

#define MAX_RSDT 35

void acpi_init();
struct acpi_madt_lapic* acpi_get_lapic_next();
struct acpi_madt_ioapic* acpi_get_ioapic_next();
struct acpi_madt_int_src* acpi_get_int_src_next();

struct acpi_hpet* acpi_get_hpet();

#endif
