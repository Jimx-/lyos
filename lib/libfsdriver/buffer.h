#ifndef _LIBFSDRIVER_BUFFER_H_
#define _LIBFSDRIVER_BUFFER_H_

#include <lyos/types.h>
#include <sys/types.h>
#include <lyos/list.h>

struct fsdriver_buffer {
    struct list_head list;
    struct list_head hash;

    char* data;
    dev_t dev;
    block_t block;

    unsigned int refcnt;
    size_t size;
    int flags;
};

void fsdriver_mark_dirty(struct fsdriver_buffer* bp);
void fsdriver_mark_clean(struct fsdriver_buffer* bp);
int fsdriver_is_clean(struct fsdriver_buffer* bp);

void fsdriver_init_buffer_cache(size_t new_size);
int fsdriver_get_block(struct fsdriver_buffer** bpp, dev_t dev, block_t block);
void fsdriver_put_block(struct fsdriver_buffer* bp);
void fsdriver_flush_dev(dev_t dev);
void fsdriver_flush_all(void);

#endif
