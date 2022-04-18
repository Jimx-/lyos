/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/list.h>
#include <lyos/sysutils.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <asm/page.h>

#include <libfsdriver/libfsdriver.h>

#include "const.h"
#include "proto.h"
#include "types.h"

#define TMPFS_INODE_HASH_LOG2 7
#define TMPFS_INODE_HASH_SIZE ((unsigned long)1 << TMPFS_INODE_HASH_LOG2)
#define TMPFS_INODE_HASH_MASK (((unsigned long)1 << TMPFS_INODE_HASH_LOG2) - 1)

static struct list_head tmpfs_inode_table[TMPFS_INODE_HASH_SIZE];

void init_inode(void)
{
    int i;
    for (i = 0; i < TMPFS_INODE_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&tmpfs_inode_table[i]);
    }
}

static ino_t get_next_ino(void)
{
    static ino_t num = TMPFS_ROOT_INODE;
    return ++num;
}

static unsigned int tmpfs_inode_gethash(dev_t dev, ino_t num)
{
    return (dev ^ num) & TMPFS_INODE_HASH_MASK;
}

static void tmpfs_inode_addhash(struct tmpfs_inode* pin)
{
    unsigned int hash = tmpfs_inode_gethash(pin->dev, pin->num);

    list_add(&pin->hash, &tmpfs_inode_table[hash]);
}

static inline void tmpfs_inode_unhash(struct tmpfs_inode* pin)
{
    list_del(&pin->hash);
}

struct tmpfs_inode* tmpfs_find_inode(dev_t dev, ino_t num)
{
    unsigned int hash = tmpfs_inode_gethash(dev, num);
    struct tmpfs_inode* pin;

    list_for_each_entry(pin, &tmpfs_inode_table[hash], hash)
    {
        if ((pin->num == num) && (pin->dev == dev)) { /* hit */
            return pin;
        }
    }

    return NULL;
}

void tmpfs_dup_inode(struct tmpfs_inode* pin) { pin->count++; }

struct tmpfs_inode* tmpfs_get_inode(dev_t dev, ino_t num)
{
    struct tmpfs_inode* pin;

    pin = tmpfs_find_inode(dev, num);
    if (!pin) return NULL;

    tmpfs_dup_inode(pin);
    return pin;
}

static void free_inode(struct tmpfs_inode* pin)
{
    if (pin->nr_pages) {
        free(pin->pages);
        pin->pages = NULL;
    }

    free(pin);
}

int tmpfs_put_inode(struct tmpfs_inode* pin)
{
    assert(pin->count > 0);

    if (--pin->count == 0) {
        if (pin->link_count == 0) {
            tmpfs_truncate_inode(pin, 0);
            tmpfs_inode_unhash(pin);
            free_inode(pin);
        }
    }

    return 0;
}

struct tmpfs_inode* tmpfs_alloc_inode(struct tmpfs_superblock* sb, ino_t num)
{
    struct tmpfs_inode* pin;

    pin = malloc(sizeof(*pin));
    if (!pin) return NULL;

    memset(pin, 0, sizeof(*pin));
    pin->dev = sb->dev;
    pin->num = num;
    pin->sb = sb;
    INIT_LIST_HEAD(&pin->children);
    pin->count = 1;

    tmpfs_inode_addhash(pin);

    return pin;
}

int tmpfs_new_inode(struct tmpfs_inode* dir_pin, const char* pathname,
                    mode_t mode, uid_t uid, gid_t gid,
                    struct tmpfs_inode** ppin)
{
    struct tmpfs_inode* pin;
    struct tmpfs_superblock* sb;
    int retval;

    if ((sb = tmpfs_get_superblock(dir_pin->dev)) == NULL) return EINVAL;

    retval = tmpfs_advance(dir_pin, pathname, &pin);

    *ppin = NULL;

    if (retval == ENOENT) {
        pin = tmpfs_alloc_inode(sb, get_next_ino());
        if (!pin) return ENOMEM;

        pin->mode = mode;
        pin->uid = uid;
        pin->gid = gid;
        pin->atime = pin->ctime = pin->mtime = now();

        if ((retval = tmpfs_search_dir(dir_pin, pathname, &pin, SD_MAKE)) !=
            OK) {
            tmpfs_put_inode(pin);
            return retval;
        }

        pin->link_count++;
        *ppin = pin;

        return 0;
    } else if (retval == OK) {
        *ppin = pin;
        retval = EEXIST;
    }

    return retval;
}

int tmpfs_putinode(dev_t dev, ino_t num, unsigned int count)
{
    struct tmpfs_inode* pin;

    pin = tmpfs_find_inode(dev, num);
    if (!pin) return EINVAL;

    assert(count <= pin->count);

    pin->count -= count - 1;
    tmpfs_put_inode(pin);
    return 0;
}

int tmpfs_inode_getpage(struct tmpfs_inode* pin, unsigned int index,
                        char** page)
{
    char** new_pages;
    char* new_page;
    size_t page_count;

    page_count = pin->nr_pages;
    while (index >= page_count) {
        page_count = (page_count + 3) * 2;
    }

    if (page_count > pin->nr_pages) {
        new_pages = realloc(pin->pages, sizeof(*pin->pages) * page_count);
        if (!new_pages) return ENOMEM;

        memset(&new_pages[pin->nr_pages], 0,
               sizeof(*pin->pages) * (page_count - pin->nr_pages));
        pin->nr_pages = page_count;
        pin->pages = new_pages;
    }

    *page = pin->pages[index];
    if (*page) return 0;

    new_page = mmap(0, ARCH_PG_SIZE, PROT_READ | PROT_WRITE,
                    MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (new_page == MAP_FAILED) return ENOMEM;

    memset(new_page, 0, ARCH_PG_SIZE);

    pin->pages[index] = new_page;
    *page = new_page;

    return 0;
}

static void zero_range(struct tmpfs_inode* pin, loff_t pos, size_t len)
{
    unsigned int index;
    off_t offset;
    char* page;

    index = pos >> ARCH_PG_SHIFT;

    if (!len) return;
    if (index >= pin->nr_pages || !pin->pages[index]) return;

    page = pin->pages[index];
    offset = pos % ARCH_PG_SIZE;
    assert(offset + len <= ARCH_PG_SIZE);

    memset(&page[offset], 0, len);
}

int tmpfs_free_range(struct tmpfs_inode* pin, loff_t start, loff_t end)
{
    int zero_first, zero_last;
    unsigned int start_index, end_index;

    if (pin->nr_pages == 0) return 0;

    if (end > pin->size) end = pin->size;
    if (end <= start) return EINVAL;

    zero_first = start % ARCH_PG_SIZE;
    zero_last = end % ARCH_PG_SIZE && end < pin->size;
    if (start >> ARCH_PG_SHIFT == (end - 1) >> ARCH_PG_SHIFT &&
        (zero_last || zero_first))
        zero_range(pin, start, end - start);
    else {
        if (zero_first)
            zero_range(pin, start, ARCH_PG_SIZE - (start % ARCH_PG_SIZE));
        if (zero_last)
            zero_range(pin, end - (end % ARCH_PG_SIZE), end % ARCH_PG_SIZE);

        start_index = (start + ARCH_PG_SIZE - 1) >> ARCH_PG_SHIFT;
        end_index = end >> ARCH_PG_SHIFT;

        if (end_index > pin->nr_pages) end_index = pin->nr_pages;

        for (; start_index < end_index; start_index++) {
            if (pin->pages[start_index]) {
                munmap(pin->pages[start_index], ARCH_PG_SIZE);
                pin->pages[start_index] = NULL;
            }
        }
    }

    pin->update |= CTIME | MTIME;
    return 0;
}

int tmpfs_truncate_inode(struct tmpfs_inode* pin, off_t new_size)
{
    int retval;

    if (S_ISBLK(pin->mode) || S_ISCHR(pin->mode)) return EINVAL;

    if (new_size < pin->size) {
        retval = tmpfs_free_range(pin, new_size, pin->size);
        if (retval) return retval;
    }

    if (new_size > pin->size)
        zero_range(pin, pin->size, ARCH_PG_SIZE - (pin->size % ARCH_PG_SIZE));

    pin->size = new_size;
    pin->update |= CTIME | MTIME;

    return 0;
}
