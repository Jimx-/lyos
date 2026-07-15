#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <acpica/acpi.h>
#undef ACPI_SIG_MCFG
#undef TRUE
#undef FALSE

#include <lyos/acpi_utils.h>
#include <lyos/config.h>
#include <lyos/const.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>

#define NR_ACPI_TABLES 32
#define NR_MCFG_ENTRIES 32

struct acpi_table {
    struct acpi_table_info info;
    ACPI_TABLE_HEADER* hdr;
};

static int acpi_init(void);
static int do_get_table_info(MESSAGE* m);
static int do_copy_table(MESSAGE* m);
static int do_get_mcfg_count(MESSAGE* m);
static int do_get_mcfg_entry(MESSAGE* m);
static int register_acpica_tables(void);
static void register_mcfg_entries(ACPI_TABLE_HEADER* hdr);
static struct acpi_table* find_table(u32 signature, u32 index);
static u32 table_signature(const char signature[ACPI_NAMESEG_SIZE]);

static struct acpi_table tables[NR_ACPI_TABLES];
static int nr_tables;
static struct acpi_mcfg_entry mcfg_entries[NR_MCFG_ENTRIES];
static int nr_mcfg_entries;

int main(void)
{
    serv_register_init_fresh_callback(acpi_init);
    serv_init();

    while (TRUE) {
        MESSAGE msg;
        int src;

        send_recv(RECEIVE_ASYNC, ANY, &msg);
        src = msg.source;

        switch (msg.type) {
        case ACPI_GET_TABLE_INFO:
            msg.RETVAL = do_get_table_info(&msg);
            break;
        case ACPI_COPY_TABLE:
            msg.RETVAL = do_copy_table(&msg);
            break;
        case ACPI_GET_MCFG_COUNT:
            msg.RETVAL = do_get_mcfg_count(&msg);
            break;
        case ACPI_GET_MCFG_ENTRY:
            msg.RETVAL = do_get_mcfg_entry(&msg);
            break;
        default:
            msg.RETVAL = ENOSYS;
            break;
        }

        msg.type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, src, &msg);
    }

    return 0;
}

static int acpi_init(void)
{
    ACPI_STATUS status;
    int retval;

    nr_tables = 0;
    nr_mcfg_entries = 0;

    status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(status)) {
        printl("acpi: AcpiInitializeSubsystem failed: %s\n",
               AcpiFormatException(status));
        return EIO;
    }

    status = AcpiInitializeTables(NULL, NR_ACPI_TABLES, TRUE);
    if (ACPI_FAILURE(status)) {
        printl("acpi: AcpiInitializeTables failed: %s\n",
               AcpiFormatException(status));
        return EIO;
    }

    retval = register_acpica_tables();
    if (retval) return retval;

    printl("acpi: ACPICA backend loaded %d tables, %d MCFG entries\n", nr_tables,
           nr_mcfg_entries);

    return 0;
}

static int register_acpica_tables(void)
{
    UINT32 index;

    for (index = 0; index < NR_ACPI_TABLES; index++) {
        ACPI_TABLE_HEADER* hdr;
        ACPI_STATUS status;
        struct acpi_table* table;

        status = AcpiGetTableByIndex(index, &hdr);
        if (status == AE_BAD_PARAMETER || status == AE_NOT_FOUND) break;
        if (ACPI_FAILURE(status)) {
            printl("acpi: failed to get table %u: %s\n", index,
                   AcpiFormatException(status));
            continue;
        }

        table = &tables[nr_tables++];
        memset(table, 0, sizeof(*table));
        table->hdr = hdr;
        table->info.signature = table_signature(hdr->Signature);
        table->info.index = index;
        table->info.phys_addr = 0;
        table->info.length = hdr->Length;
        table->info.revision = hdr->Revision;

        if (table->info.signature == ACPI_SIG_MCFG) register_mcfg_entries(hdr);

        if (nr_tables == NR_ACPI_TABLES) break;
    }

    return nr_tables ? 0 : ENODEV;
}

static void register_mcfg_entries(ACPI_TABLE_HEADER* hdr)
{
    ACPI_TABLE_MCFG* mcfg = (ACPI_TABLE_MCFG*)hdr;
    ACPI_MCFG_ALLOCATION* alloc;
    ACPI_MCFG_ALLOCATION* end;

    if (hdr->Length < sizeof(*mcfg)) return;

    alloc = (ACPI_MCFG_ALLOCATION*)((char*)mcfg + sizeof(*mcfg));
    end = (ACPI_MCFG_ALLOCATION*)((char*)mcfg + hdr->Length);

    while (alloc < end && nr_mcfg_entries < NR_MCFG_ENTRIES) {
        struct acpi_mcfg_entry* entry = &mcfg_entries[nr_mcfg_entries++];

        entry->base_addr = alloc->Address;
        entry->segment = alloc->PciSegment;
        entry->start_bus = alloc->StartBusNumber;
        entry->end_bus = alloc->EndBusNumber;
        entry->reserved = alloc->Reserved;

        alloc++;
    }
}

static int do_get_table_info(MESSAGE* m)
{
    struct acpi_table* table;
    struct acpi_table_info info;

    table = find_table((u32)m->u.m3.m3l1, (u32)m->u.m3.m3i2);
    if (!table) return ENODEV;

    info = table->info;
    if (data_copy(m->source, m->u.m3.m3p1, SELF, &info, sizeof(info)))
        return EFAULT;

    return 0;
}

static int do_copy_table(MESSAGE* m)
{
    struct acpi_table* table;
    size_t len;

    table = find_table((u32)m->u.m3.m3l1, (u32)m->u.m3.m3i2);
    if (!table) return ENODEV;
    if (!table->hdr) return ENODEV;

    len = (size_t)m->u.m3.m3l2;
    if (len < table->info.length) return EINVAL;

    if (data_copy(m->source, m->u.m3.m3p1, SELF, table->hdr,
                  table->info.length))
        return EFAULT;

    m->u.m3.m3l2 = table->info.length;
    return 0;
}

static int do_get_mcfg_count(MESSAGE* m)
{
    m->u.m3.m3i2 = nr_mcfg_entries;
    return 0;
}

static int do_get_mcfg_entry(MESSAGE* m)
{
    struct acpi_mcfg_entry entry;
    u32 index = (u32)m->u.m3.m3i2;

    if (index >= (u32)nr_mcfg_entries) return ENODEV;

    entry = mcfg_entries[index];
    if (data_copy(m->source, m->u.m3.m3p1, SELF, &entry, sizeof(entry)))
        return EFAULT;

    return 0;
}

static struct acpi_table* find_table(u32 signature, u32 index)
{
    int i;
    u32 seen = 0;

    for (i = 0; i < nr_tables; i++) {
        if (tables[i].info.signature != signature) continue;
        if (seen == index) return &tables[i];
        seen++;
    }

    return NULL;
}

static u32 table_signature(const char signature[ACPI_NAMESEG_SIZE])
{
    return ACPI_SIG(signature[0], signature[1], signature[2], signature[3]);
}
