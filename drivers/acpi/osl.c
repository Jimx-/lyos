#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <acpica/acpi.h>
#undef TRUE
#undef FALSE

#include <asm/page.h>
#include <lyos/const.h>
#include <lyos/portio.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>

#define NR_ACPI_OS_MAPS 64

struct acpi_os_semaphore {
    UINT32 units;
    UINT32 max_units;
};

struct acpi_os_cache {
    UINT16 object_size;
};

struct acpi_os_map {
    void* logical;
    void* base;
    size_t length;
};

static struct acpi_os_map maps[NR_ACPI_OS_MAPS];

static size_t page_roundup(size_t value)
{
    return (value + ARCH_PG_SIZE - 1) & ~(ARCH_PG_SIZE - 1);
}

ACPI_STATUS AcpiOsInitialize(void)
{
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate(void)
{
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void)
{
    ACPI_PHYSICAL_ADDRESS address = 0;

    if (ACPI_FAILURE(AcpiFindRootPointer(&address))) return 0;

    return address;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* init_val,
                                     ACPI_STRING* new_val)
{
    (void)init_val;
    *new_val = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* existing_table,
                                ACPI_TABLE_HEADER** new_table)
{
    (void)existing_table;
    *new_table = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER* existing_table,
                                        ACPI_PHYSICAL_ADDRESS* new_address,
                                        UINT32* new_table_length)
{
    (void)existing_table;
    *new_address = 0;
    *new_table_length = 0;
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* out_handle)
{
    *out_handle = NULL;
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK handle)
{
    (void)handle;
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK handle)
{
    (void)handle;
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK handle, ACPI_CPU_FLAGS flags)
{
    (void)handle;
    (void)flags;
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max_units, UINT32 initial_units,
                                  ACPI_SEMAPHORE* out_handle)
{
    struct acpi_os_semaphore* sem;

    sem = malloc(sizeof(*sem));
    if (!sem) return AE_NO_MEMORY;

    sem->units = initial_units;
    sem->max_units = max_units;
    *out_handle = sem;

    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE handle)
{
    free(handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE handle, UINT32 units,
                                UINT16 timeout)
{
    struct acpi_os_semaphore* sem = handle;

    (void)timeout;

    if (!sem || sem->units < units) return AE_TIME;

    sem->units -= units;
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE handle, UINT32 units)
{
    struct acpi_os_semaphore* sem = handle;

    if (!sem) return AE_BAD_PARAMETER;
    if (sem->units + units > sem->max_units) return AE_LIMIT;

    sem->units += units;
    return AE_OK;
}

void* AcpiOsAllocate(ACPI_SIZE size)
{
    return malloc(size);
}

void AcpiOsFree(void* memory)
{
    free(memory);
}

void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS where, ACPI_SIZE length)
{
    size_t offset;
    size_t map_length;
    void* ptr;
    int i;

    ptr = mm_map_phys(SELF, (phys_bytes)where, (size_t)length, 0);
    if (ptr == MAP_FAILED) return NULL;

    offset = (size_t)where & (ARCH_PG_SIZE - 1);
    map_length = page_roundup((size_t)length + offset);

    for (i = 0; i < NR_ACPI_OS_MAPS; i++) {
        if (maps[i].logical) continue;

        maps[i].logical = ptr;
        maps[i].base = (char*)ptr - offset;
        maps[i].length = map_length;
        break;
    }

    if (i == NR_ACPI_OS_MAPS) {
        munmap((char*)ptr - offset, map_length);
        return NULL;
    }

    return ptr;
}

void AcpiOsUnmapMemory(void* logical_address, ACPI_SIZE size)
{
    int i;

    (void)size;

    if (!logical_address) return;

    for (i = 0; i < NR_ACPI_OS_MAPS; i++) {
        if (maps[i].logical != logical_address) continue;

        munmap(maps[i].base, maps[i].length);
        memset(&maps[i], 0, sizeof(maps[i]));
        return;
    }
}

ACPI_STATUS AcpiOsCreateCache(char* cache_name, UINT16 object_size,
                              UINT16 max_depth, ACPI_CACHE_T** return_cache)
{
    struct acpi_os_cache* cache;

    (void)cache_name;
    (void)max_depth;

    cache = malloc(sizeof(*cache));
    if (!cache) return AE_NO_MEMORY;

    cache->object_size = object_size;
    *return_cache = (ACPI_CACHE_T*)cache;

    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteCache(ACPI_CACHE_T* cache)
{
    free(cache);
    return AE_OK;
}

ACPI_STATUS AcpiOsPurgeCache(ACPI_CACHE_T* cache)
{
    (void)cache;
    return AE_OK;
}

void* AcpiOsAcquireObject(ACPI_CACHE_T* cache)
{
    struct acpi_os_cache* os_cache = (struct acpi_os_cache*)cache;

    if (!os_cache) return NULL;

    return calloc(1, os_cache->object_size);
}

ACPI_STATUS AcpiOsReleaseObject(ACPI_CACHE_T* cache, void* object)
{
    (void)cache;
    AcpiOsFree(object);
    return AE_OK;
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 interrupt_number,
                                          ACPI_OSD_HANDLER service_routine,
                                          void* context)
{
    (void)interrupt_number;
    (void)service_routine;
    (void)context;
    return AE_SUPPORT;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 interrupt_number,
                                         ACPI_OSD_HANDLER service_routine)
{
    (void)interrupt_number;
    (void)service_routine;
    return AE_OK;
}

ACPI_THREAD_ID AcpiOsGetThreadId(void)
{
    return 1;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type,
                          ACPI_OSD_EXEC_CALLBACK function, void* context)
{
    (void)type;
    function(context);
    return AE_OK;
}

void AcpiOsWaitEventsComplete(void)
{
}

void AcpiOsSleep(UINT64 milliseconds)
{
    usleep((useconds_t)milliseconds * 1000);
}

void AcpiOsStall(UINT32 microseconds)
{
    usleep(microseconds);
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS address, UINT32* value, UINT32 width)
{
    u32 tmp = 0;
    int type;

    switch (width) {
    case 8:
        type = PIO_BYTE;
        break;
    case 16:
        type = PIO_WORD;
        break;
    case 32:
        type = PIO_LONG;
        break;
    default:
        return AE_BAD_PARAMETER;
    }

    if (portio_in((int)address, &tmp, type)) return AE_ERROR;

    *value = tmp;
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS address, UINT32 value, UINT32 width)
{
    int type;

    switch (width) {
    case 8:
        type = PIO_BYTE;
        break;
    case 16:
        type = PIO_WORD;
        break;
    case 32:
        type = PIO_LONG;
        break;
    default:
        return AE_BAD_PARAMETER;
    }

    if (portio_out((int)address, value, type)) return AE_ERROR;

    return AE_OK;
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS address, UINT64* value,
                             UINT32 width)
{
    void* ptr;
    size_t size = width / 8;

    if (width != 8 && width != 16 && width != 32 && width != 64)
        return AE_BAD_PARAMETER;

    ptr = AcpiOsMapMemory(address, size);
    if (!ptr) return AE_NO_MEMORY;

    switch (width) {
    case 8:
        *value = *(volatile UINT8*)ptr;
        break;
    case 16:
        *value = *(volatile UINT16*)ptr;
        break;
    case 32:
        *value = *(volatile UINT32*)ptr;
        break;
    case 64:
        *value = *(volatile UINT64*)ptr;
        break;
    }

    AcpiOsUnmapMemory(ptr, size);
    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS address, UINT64 value,
                              UINT32 width)
{
    void* ptr;
    size_t size = width / 8;

    if (width != 8 && width != 16 && width != 32 && width != 64)
        return AE_BAD_PARAMETER;

    ptr = AcpiOsMapMemory(address, size);
    if (!ptr) return AE_NO_MEMORY;

    switch (width) {
    case 8:
        *(volatile UINT8*)ptr = (UINT8)value;
        break;
    case 16:
        *(volatile UINT16*)ptr = (UINT16)value;
        break;
    case 32:
        *(volatile UINT32*)ptr = (UINT32)value;
        break;
    case 64:
        *(volatile UINT64*)ptr = value;
        break;
    }

    AcpiOsUnmapMemory(ptr, size);
    return AE_OK;
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* pci_id, UINT32 reg,
                                       UINT64* value, UINT32 width)
{
    (void)pci_id;
    (void)reg;
    (void)width;
    *value = 0;
    return AE_SUPPORT;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* pci_id, UINT32 reg,
                                        UINT64 value, UINT32 width)
{
    (void)pci_id;
    (void)reg;
    (void)value;
    (void)width;
    return AE_SUPPORT;
}

BOOLEAN AcpiOsReadable(void* pointer, ACPI_SIZE length)
{
    return pointer != NULL && length > 0;
}

BOOLEAN AcpiOsWritable(void* pointer, ACPI_SIZE length)
{
    return pointer != NULL && length > 0;
}

UINT64 AcpiOsGetTimer(void)
{
    clock_t ticks = 0;

    if (get_ticks(&ticks, NULL)) return 0;

    return (UINT64)ticks * (10000000ULL / DEFAULT_HZ);
}

ACPI_STATUS AcpiOsSignal(UINT32 function, void* info)
{
    (void)function;
    (void)info;
    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 sleep_state, UINT32 rega_value,
                             UINT32 regb_value)
{
    (void)sleep_state;
    (void)rega_value;
    (void)regb_value;
    return AE_SUPPORT;
}

void AcpiOsPrintf(const char* format, ...)
{
    va_list ap;

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

void AcpiOsVprintf(const char* format, va_list args)
{
    vprintf(format, args);
}
