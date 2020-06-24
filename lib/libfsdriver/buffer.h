#ifndef _LIBFSDRIVER_BUFFER_H_
#define _LIBFSDRIVER_BUFFER_H_

#include <lyos/type.h>
#include <sys/types.h>
#include <lyos/list.h>

struct fsd_buffer {
    struct list_head list;
    struct list_head hash;

    char* data;
    dev_t dev;
    block_t block;

    unsigned int refcnt;
    size_t size;
    int flags;
};

void fsd_mark_dirty(struct fsd_buffer* bp);
void fsd_mark_clean(struct fsd_buffer* bp);
int fsd_is_clean(struct fsd_buffer* bp);

void fsd_init_buffer_cache(size_t new_size);
int fsd_get_block(struct fsd_buffer** bpp, dev_t dev, block_t block);
void fsd_put_block(struct fsd_buffer* bp);
void fsd_flush_dev(dev_t dev);
void fsd_flush_all(void);

#endif
