#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/driver.h>
#include <lyos/proto.h>
#include <sys/syslimits.h>

#include <libdevman/libdevman.h>
#include <libsysfs/libsysfs.h>

int dm_bus_get_or_create(const char* name, bus_type_id_t* idp)
{
    char label[PATH_MAX];
    unsigned int id;
    int retval;

    snprintf(label, PATH_MAX, "bus.%s.handle", name);

    for (;;) {
        retval = sysfs_retrieve_u32(label, &id);
        if (retval == 0) {
            *idp = id;
            return 0;
        }

        if (retval != ENOENT) return retval;

        retval = dm_bus_register(name, idp);
        if (retval != EEXIST) break;
    }

    return retval;
}

int dm_class_get_or_create(const char* name, class_id_t* idp)
{
    char label[PATH_MAX];
    unsigned int id;
    int retval;

    snprintf(label, PATH_MAX, "class.%s.handle", name);

    for (;;) {
        retval = sysfs_retrieve_u32(label, &id);
        if (retval == 0) {
            *idp = id;
            return 0;
        }

        if (retval != ENOENT) return retval;

        retval = dm_class_register(name, idp);
        if (retval != EEXIST) break;
    }

    return retval;
}
