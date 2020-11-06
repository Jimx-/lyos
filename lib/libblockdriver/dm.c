#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>
#include <sys/mman.h>

#include <libblockdriver/libblockdriver.h>
#include <libdevman/libdevman.h>

static class_id_t block_class_id = NO_CLASS_ID;

int blockdriver_device_register(struct device_info* devinf, device_id_t* id)
{
    int retval;

    if (block_class_id == NO_CLASS_ID) {
        retval = dm_class_get_or_create("block", &block_class_id);
        if (retval) return retval;
    }

    devinf->class = block_class_id;

    return dm_device_register(devinf, id);
}
