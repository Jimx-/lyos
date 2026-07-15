#include <lyos/types.h>
#include <lyos/ipc.h>
#include <errno.h>
#include <lyos/acpi_utils.h>
#include <lyos/const.h>
#include <lyos/service.h>
#include "libsysfs/libsysfs.h"

static endpoint_t acpi_endpoint = NO_TASK;

static int acpi_sendrec(MESSAGE* msg)
{
    int retval;
    u32 endpoint;

    if (acpi_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.acpi.endpoint", &endpoint);
        if (retval) return retval;
        acpi_endpoint = (endpoint_t)endpoint;
    }

    return send_recv(BOTH, acpi_endpoint, msg);
}

int acpi_get_table_info(u32 signature, u32 index, struct acpi_table_info* info)
{
    MESSAGE msg;
    int retval;

    msg.type = ACPI_GET_TABLE_INFO;
    msg.u.m3.m3l1 = signature;
    msg.u.m3.m3i2 = index;
    msg.u.m3.m3p1 = info;

    retval = acpi_sendrec(&msg);
    if (retval) return retval;
    return msg.RETVAL;
}

int acpi_copy_table(u32 signature, u32 index, void* buf, size_t len)
{
    MESSAGE msg;
    int retval;

    msg.type = ACPI_COPY_TABLE;
    msg.u.m3.m3l1 = signature;
    msg.u.m3.m3i2 = index;
    msg.u.m3.m3p1 = buf;
    msg.u.m3.m3l2 = len;

    retval = acpi_sendrec(&msg);
    if (retval) return retval;
    return msg.RETVAL;
}

int acpi_get_mcfg_count(void)
{
    MESSAGE msg;
    int retval;

    msg.type = ACPI_GET_MCFG_COUNT;

    retval = acpi_sendrec(&msg);
    if (retval) return retval;
    if (msg.RETVAL) return -msg.RETVAL;

    return msg.u.m3.m3i2;
}

int acpi_get_mcfg_entry(u32 index, struct acpi_mcfg_entry* entry)
{
    MESSAGE msg;
    int retval;

    msg.type = ACPI_GET_MCFG_ENTRY;
    msg.u.m3.m3i2 = index;
    msg.u.m3.m3p1 = entry;

    retval = acpi_sendrec(&msg);
    if (retval) return retval;
    return msg.RETVAL;
}
