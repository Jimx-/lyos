#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <errno.h>
#include <string.h>
#include <asm/page.h>

#include <libfsdriver/libfsdriver.h>

#include "archive.h"

int initfs_read_bytes(dev_t dev, off_t offset, void* buf, size_t len)
{
    char* dst = buf;

    while (len) {
        struct fsdriver_buffer* bp;
        size_t block = offset / ARCH_PG_SIZE;
        size_t block_off = offset % ARCH_PG_SIZE;
        size_t chunk = min(ARCH_PG_SIZE - block_off, len);
        int retval = fsdriver_get_block(&bp, dev, block);

        if (retval != 0) return retval;
        memcpy(dst, bp->data + block_off, chunk);
        fsdriver_put_block(bp);
        dst += chunk;
        offset += chunk;
        len -= chunk;
    }
    return 0;
}
