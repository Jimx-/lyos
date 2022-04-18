#include <lyos/ipc.h>
#include <errno.h>
#include <lyos/const.h>

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
