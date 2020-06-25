#include <lyos/config.h>
#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <lyos/fs.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <sys/mman.h>
#include <asm/page.h>

#include <libfsdriver/libfsdriver.h>
#include <libbdev/libbdev.h>

#define BF_DIRTY 0x1

#define BUFFER_HASH_LOG2 7
#define BUFFER_HASH_SIZE ((unsigned long)1 << BUFFER_HASH_LOG2)
#define BUFFER_HASH_MASK (BUFFER_HASH_SIZE - 1)

static struct fsd_buffer* bufs;
static struct list_head buf_hash[BUFFER_HASH_SIZE];
static struct list_head lru_head;
static size_t cache_size = 0;
static size_t fs_block_size = ARCH_PG_SIZE;

void fsd_mark_dirty(struct fsd_buffer* bp) { bp->flags |= BF_DIRTY; }

void fsd_mark_clean(struct fsd_buffer* bp) { bp->flags &= ~BF_DIRTY; }

int fsd_is_clean(struct fsd_buffer* bp) { return !(bp->flags & BF_DIRTY); }

static inline void remove_lru(struct fsd_buffer* bp) { list_del(&bp->list); }

static int get_block(struct fsd_buffer** bpp, dev_t dev, block_t block,
                     size_t block_size);
static void put_block(struct fsd_buffer* bp);
static int read_block(struct fsd_buffer* bp, size_t block_size);

static struct fsd_buffer* find_block(dev_t dev, block_t block)
{
    size_t hash = block & BUFFER_HASH_MASK;
    struct fsd_buffer* bp;
    list_for_each_entry(bp, &buf_hash[hash], hash)
    {
        if (bp->dev == dev && bp->block == block) {
            return bp;
        }
    }

    return NULL;
}

static int alloc_block(struct fsd_buffer* bp, size_t block_size)
{
    if (bp->data) {
        if (bp->size == block_size) {
            return 0;
        }

        munmap(bp->data, bp->size);
        bp->data = NULL;
    }

    bp->data = mmap(0, block_size, PROT_READ | PROT_WRITE,
                    MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (bp->data == MAP_FAILED) {
        bp->data = NULL;
        bp->size = 0;
        return ENOMEM;
    }

    bp->size = block_size;

    return 0;
}

static int get_block(struct fsd_buffer** bpp, dev_t dev, block_t block,
                     size_t block_size)
{
    struct fsd_buffer* bp;
    size_t hash;
    int retval;

    /* search for the block in the cache */
    bp = find_block(dev, block);
    if (bp) {
        if (bp->size != block_size) {
            return EIO;
        }

        if (bp->refcnt == 0) {
            remove_lru(bp);
        }

        bp->refcnt++;

        *bpp = bp;
        return 0;
    }

    if (list_empty(&lru_head)) {
        panic("all buffers are in use");
    }

    /* get a free block from LRU */
    bp = list_entry(lru_head.prev, struct fsd_buffer, list);
    remove_lru(bp);
    list_del(&bp->hash);

    if (bp->dev != NO_DEV) {
        if (!fsd_is_clean(bp)) {
            fsd_flush_dev(bp->dev);
        }
    }

    bp->flags = 0;
    bp->dev = dev;
    bp->block = block;
    assert(bp->refcnt == 0);
    bp->refcnt++;

    retval = alloc_block(bp, block_size);
    if (retval) {
        bp->dev = NO_DEV;
        put_block(bp);

        return retval;
    }

    retval = read_block(bp, block_size);
    if (retval) {
        bp->dev = NO_DEV;
        put_block(bp);
    }

    hash = block & BUFFER_HASH_MASK;
    list_add(&bp->hash, &buf_hash[hash]);

    *bpp = bp;

    return 0;
}

static void put_block(struct fsd_buffer* bp)
{
    bp->refcnt--;

    if (bp->refcnt > 0) {
        return;
    }

    if (bp->dev == NO_DEV) {
        list_add(&bp->list, lru_head.prev);
    } else {
        list_add(&bp->list, &lru_head);
    }
}

static int read_block(struct fsd_buffer* bp, size_t block_size)
{
    loff_t pos;
    ssize_t retval;

    pos = (loff_t)fs_block_size * bp->block;
    retval = bdev_read(bp->dev, pos, bp->data, block_size, SELF);

    if (retval != block_size) {
        if (retval > 0) {
            return EIO;
        }

        return retval;
    }

    return 0;
}

static int block_cmp_fn(const void* a, const void* b)
{
    struct fsd_buffer* lhs = *(struct fsd_buffer**)a;
    struct fsd_buffer* rhs = *(struct fsd_buffer**)b;

    return (int)lhs->block - (int)rhs->block;
}

static void scatter_gather(dev_t dev, struct fsd_buffer** buffers, size_t count,
                           int do_write)
{
    struct fsd_buffer* bp;
    struct iovec* iov;
    struct iovec iovec[NR_IOREQS];
    size_t nbufs, niovs;
    loff_t pos;
    int i;
    ssize_t retval;

    if (count == 0) return;

    qsort(buffers, count, sizeof(*buffers), block_cmp_fn);

    while (count > 0) {
        nbufs = 0;
        niovs = 0;

        for (iov = iovec; nbufs < count; nbufs++) {
            bp = buffers[nbufs];

            if (bp->block != buffers[0]->block + nbufs) {
                break;
            }

            iov->iov_base = bp->data;
            iov->iov_len = bp->size;
            iov++;
            niovs++;
        }

        pos = (loff_t)buffers[0]->block * fs_block_size;
        if (do_write) {
            retval = bdev_writev(dev, pos, SELF, iovec, niovs);
        } else {
            retval = bdev_readv(dev, pos, SELF, iovec, niovs);
        }

        for (i = 0; i < nbufs; i++) {
            bp = buffers[i];

            if (retval < bp->size) {
                break;
            }

            if (do_write) {
                fsd_mark_clean(bp);
            }

            retval -= bp->size;
        }

        buffers += i;
        count -= i;

        if (do_write && i == 0) {
            break;
        }
    }
}

void fsd_init_buffer_cache(size_t new_size)
{
    int i;
    struct fsd_buffer* bp;

    if (cache_size > 0) {
        for (bp = bufs; bp < &bufs[cache_size]; bp++) {
            if (bp->data) {
                munmap(bp->data, bp->size);
            }
        }
    }

    if (bufs) {
        free(bufs);
    }

    bufs = calloc(sizeof(bufs[0]), new_size);
    if (!bufs) {
        panic("cannot allocate buffer list");
    }

    cache_size = new_size;

    INIT_LIST_HEAD(&lru_head);

    for (bp = bufs; bp < &bufs[cache_size]; bp++) {
        bp->dev = NO_DEV;
        bp->data = NULL;
        bp->size = 0;
        bp->flags = 0;

        INIT_LIST_HEAD(&bp->list);
        INIT_LIST_HEAD(&bp->hash);

        list_add(&bp->list, &lru_head);
    }

    for (i = 0; i < BUFFER_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&buf_hash[i]);
    }
}

int fsd_get_block(struct fsd_buffer** bpp, dev_t dev, block_t block)
{
    return get_block(bpp, dev, block, fs_block_size);
}

void fsd_put_block(struct fsd_buffer* bp)
{
    if (!bp) return;

    put_block(bp);
}

void fsd_flush_dev(dev_t dev)
{
    struct fsd_buffer* bp;
    static struct fsd_buffer** dirty_list;
    static size_t dirty_list_size = 0;
    size_t count = 0;

    if (dirty_list_size != cache_size) {
        if (dirty_list_size > 0) {
            free(dirty_list);
        }

        dirty_list = calloc(cache_size, sizeof(*dirty_list));
        dirty_list_size = cache_size;
    }

    for (bp = bufs; bp < &bufs[cache_size]; bp++) {
        if (!fsd_is_clean(bp) && bp->refcnt > 0) {
            printl("buffer not flushed due to refcnt\n");
        }
        if (!fsd_is_clean(bp) && bp->dev == dev && bp->refcnt == 0) {
            dirty_list[count++] = bp;
        }
    }

    scatter_gather(dev, dirty_list, count, 1);
}

void fsd_flush_all(void)
{
    struct fsd_buffer* bp;

    for (bp = bufs; bp < &bufs[cache_size]; bp++) {
        if (bp->dev != NO_DEV && !fsd_is_clean(bp)) {
            fsd_flush_dev(bp->dev);
        }
    }
}
