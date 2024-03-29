/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "lyos/const.h"
#include "string.h"
#include <kernel/proto.h>
#include "acpi.h"

static struct acpi_rsdp acpi_rsdp;

static struct acpi_rsdt {
    struct acpi_sdt_header hdr;
    u32 data[MAX_RSDT];
} rsdt;

static struct {
    char signature[ACPI_SDT_SIGNATURE_LEN + 1];
    size_t length;
} sdt_trans[MAX_RSDT];

static int sdt_count;

static int acpi_find_rsdp();
static int acpi_read_sdt_at(unsigned long addr, struct acpi_sdt_header* sdt_hdr,
                            size_t size, const char* name);

void acpi_init()
{
    int s, i;

    if (!acpi_find_rsdp()) {
        printk("ACPI: RSDP not found!\n");
        return;
    }

    s = acpi_read_sdt_at((unsigned long)acpi_rsdp.rsdt_addr,
                         (struct acpi_sdt_header*)&rsdt,
                         sizeof(struct acpi_rsdt), ACPI_SDT_SIGNATURE(RSDT));

    sdt_count = (s - sizeof(struct acpi_sdt_header)) / sizeof(u32);

    for (i = 0; i < sdt_count; i++) {
        struct acpi_sdt_header hdr;
        int j;
        memcpy(&hdr, (void*)(unsigned long)rsdt.data[i],
               sizeof(struct acpi_sdt_header));

        for (j = 0; j < ACPI_SDT_SIGNATURE_LEN; j++)
            sdt_trans[i].signature[j] = hdr.signature[j];
        sdt_trans[i].signature[ACPI_SDT_SIGNATURE_LEN] = '\0';
        sdt_trans[i].length = hdr.length;
        printk("ACPI: %s %08x %05x (v0%d %.6s %.8s %08x)\n",
               sdt_trans[i].signature, rsdt.data[i], sdt_trans[i].length,
               hdr.revision, hdr.oemid, hdr.oem_table_id, hdr.oem_revision);
    }
}

static int acpi_checksum(char* ptr, u32 length)
{
    u8 total = 0;
    u32 i;
    for (i = 0; i < length; i++)
        total += ((u8*)ptr)[i];
    return !total;
}

static int acpi_find_rsdp_ptr(void* start, void* end)
{
    unsigned long addr;
    for (addr = (unsigned long)start; addr < (unsigned long)end; addr += 16) {
        memcpy(&acpi_rsdp, (void*)addr, sizeof(struct acpi_rsdp));
        if (!acpi_checksum((char*)addr, 20)) continue;
        if (!strncmp(acpi_rsdp.signature, "RSD PTR ", 8)) return 1;
    }

    return 0;
}

static int acpi_check_signature(const char* orig, const char* match)
{
    return strncmp(orig, match, ACPI_SDT_SIGNATURE_LEN);
}

static int acpi_read_sdt_at(unsigned long addr, struct acpi_sdt_header* sdt_hdr,
                            size_t size, const char* name)
{
    struct acpi_sdt_header hdr;

    if (sdt_hdr == NULL) {
        memcpy(&hdr, (void*)addr, sizeof(struct acpi_sdt_header));
        return hdr.length;
    }

    memcpy(sdt_hdr, (void*)addr, sizeof(struct acpi_sdt_header));

    if (acpi_check_signature(sdt_hdr->signature, name)) {
        printk("ACPI: %s signature doesn't match!\n", name);
        return -1;
    }

    if (size < sdt_hdr->length) {
        printk("ACPI: buffer too small for %s\n", name);
        return -1;
    }

    memcpy(sdt_hdr, (void*)addr, size);

    if (!acpi_checksum((char*)sdt_hdr, sdt_hdr->length)) {
        printk("ACPI: %s checksum does not match\n", name);
        return -1;
    }

    return sdt_hdr->length;
}

static int acpi_find_rsdp()
{
    u16 ebda;

    memcpy(&ebda, (void*)0x40E, sizeof(ebda));
    if (ebda) {
        ebda <<= 4;
        if (acpi_find_rsdp_ptr((void*)(unsigned long)ebda,
                               (void*)((unsigned long)ebda + 0x400))) {
            return 1;
        }
    }

    if (acpi_find_rsdp_ptr((void*)0xE0000UL, (void*)0x100000UL)) {
        return 1;
    }

    return 0;
}

unsigned long acpi_get_table_base(const char* name)
{
    int i;

    for (i = 0; i < sdt_count; i++) {
        if (strncmp(name, sdt_trans[i].signature, ACPI_SDT_SIGNATURE_LEN) == 0)
            return rsdt.data[i];
    }

    return 0;
}

static void* acpi_madt_get_typed_item(struct acpi_madt_hdr* hdr,
                                      unsigned char type, unsigned idx)
{
    u8 *t, *end;
    int i;

    t = (u8*)hdr + sizeof(struct acpi_madt_hdr);
    end = (u8*)hdr + hdr->hdr.length;

    i = 0;
    while (t < end) {
        if (type == ((struct acpi_madt_item_hdr*)t)->type) {
            if (i == idx)
                return t;
            else
                i++;
        }
        t += ((struct acpi_madt_item_hdr*)t)->length;
    }

    return NULL;
}

struct acpi_madt_lapic* acpi_get_lapic_next()
{
    static unsigned idx = 0;
    static struct acpi_madt_hdr* madt_hdr;

    struct acpi_madt_lapic* ret;

    if (idx == 0) {
        madt_hdr = (struct acpi_madt_hdr*)acpi_get_table_base("APIC");
        if (madt_hdr == NULL) return NULL;
    }

    for (;;) {
        ret = (struct acpi_madt_lapic*)acpi_madt_get_typed_item(
            madt_hdr, ACPI_MADT_TYPE_LAPIC, idx);
        if (!ret) break;

        idx++;

        /* report only usable CPUs */
        if (ret->flags & 1) break;
    }

    return ret;
}

struct acpi_madt_ioapic* acpi_get_ioapic_next()
{
    static unsigned idx = 0;
    static struct acpi_madt_hdr* madt_hdr;

    struct acpi_madt_ioapic* ret;

    if (idx == 0) {
        madt_hdr = (struct acpi_madt_hdr*)acpi_get_table_base("APIC");
        if (madt_hdr == NULL) return NULL;
    }

    ret = (struct acpi_madt_ioapic*)acpi_madt_get_typed_item(
        madt_hdr, ACPI_MADT_TYPE_IOAPIC, idx);
    if (ret) idx++;

    return ret;
}

struct acpi_madt_int_src* acpi_get_int_src_next()
{
    static unsigned idx = 0;
    static struct acpi_madt_hdr* madt_hdr;

    struct acpi_madt_ioapic* ret;

    if (idx == 0) {
        madt_hdr = (struct acpi_madt_hdr*)acpi_get_table_base("APIC");
        if (madt_hdr == NULL) return NULL;
    }

    ret = (struct acpi_madt_ioapic*)acpi_madt_get_typed_item(
        madt_hdr, ACPI_MADT_TYPE_INT_SRC, idx);
    if (ret) idx++;

    return (struct acpi_madt_int_src*)ret;
}

struct acpi_hpet* acpi_get_hpet()
{
    return (struct acpi_hpet*)acpi_get_table_base("HPET");
}
