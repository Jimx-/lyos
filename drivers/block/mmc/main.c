#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>

#include <libblockdriver/libblockdriver.h>

#define NAME "mmcblk"

static struct blockdriver mmc_driver = {};

static int mmc_init(void)
{
    printl(NAME ": MMC driver is running.\n");

    return 0;
}

int main(int argc, char** argv)
{
    serv_register_init_fresh_callback(mmc_init);
    serv_init();

    blockdriver_task(&mmc_driver);

    return 0;
}
