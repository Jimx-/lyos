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
#include <lyos/const.h>
#include <lyos/vm.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <asm/page.h>

#include <lyos/dmapool.h>

/* The initial backend assumes coherent identity DMA: device DMA addresses
 * equal physical addresses and the CPU is cache-coherent with DMA.  This
 * holds on the x86 PC platform.  On other platforms creation fails with
 * EOPNOTSUPP rather than silently assuming correctness. */
#if defined(CONFIG_X86_64) || defined(CONFIG_X86_32)
#define DMA_POOL_BACKEND 1
#else
#define DMA_POOL_BACKEND 0
#endif

struct dma_pool_slot {
    void* cpu;
    phys_bytes dma;
    int in_use;
    struct dma_pool_slot* next_free;
};

struct dma_pool_chunk {
    void* map_base;
    size_t map_len;
    phys_bytes dma_base;
    struct dma_pool_slot* slots;
    unsigned int num_slots;
    struct dma_pool_chunk* next;
};

struct dma_pool {
    char* name;
    size_t size;
    size_t align;
    size_t boundary;
    phys_bytes dma_mask;
    size_t stride;
    size_t chunk_len;
    struct dma_pool_chunk* chunks;
    struct dma_pool_slot* free_list;
    unsigned int active;
};

static int is_pow2(size_t n)
{
    return n != 0 && (n & (n - 1)) == 0;
}

static int align_up_sz(size_t v, size_t a, size_t* out)
{
    size_t rem = v & (a - 1);

    if (rem) {
        size_t pad = a - rem;
        if (v > (size_t)-1 - pad) return 0;
        v += pad;
    }

    *out = v;
    return 1;
}

#if DMA_POOL_BACKEND
/* Translate a fresh backing mapping page by page.  umap's length argument
 * does not establish contiguity, the kernel stores the va2pa failure
 * sentinel (phys_bytes)-1 even when reporting success, and address
 * arithmetic must not wrap, so every page is checked here once per backing
 * allocation. */
static int backing_translate(void* base, size_t len, phys_bytes* dma_basep)
{
    phys_bytes first = 0;
    size_t off;

    for (off = 0; off < len; off += ARCH_PG_SIZE) {
        size_t step = (len - off < ARCH_PG_SIZE) ? len - off : ARCH_PG_SIZE;
        vir_bytes va = (vir_bytes)base + off;
        phys_bytes pa;

        if (umap(SELF, UMT_VADDR, va, step, &pa)) return EFAULT;
        if (pa == (phys_bytes)-1) return EFAULT;

        if (off == 0) {
            first = pa;
            if (first > (phys_bytes)-1 - (len - 1)) return EFAULT;
        } else if (pa != first + off) {
            return EFAULT;
        }
    }

    *dma_basep = first;
    return 0;
}

static int backing_alloc(size_t len, void** basep, phys_bytes* dma_basep)
{
    void* base;
    phys_bytes dma_base;
    int retval;

    base = mmap(NULL, len, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | MAP_CONTIG, -1, 0);
    if (base == MAP_FAILED) return ENOMEM;

    retval = backing_translate(base, len, &dma_base);
    if (retval) {
        munmap(base, len);
        return retval;
    }

    *basep = base;
    *dma_basep = dma_base;
    return 0;
}
#else
static int backing_alloc(size_t len, void** basep, phys_bytes* dma_basep)
{
    (void)len;
    (void)basep;
    (void)dma_basep;
    return EOPNOTSUPP;
}
#endif

/* Carve slots out of a new backing chunk using the actual DMA base for
 * boundary checks, and publish the chunk only when it is complete. */
static int dma_pool_grow(struct dma_pool* pool)
{
    struct dma_pool_chunk* chunk;
    struct dma_pool_slot* slot;
    void* base;
    phys_bytes dma_base;
    size_t max_slots;
    size_t off;
    unsigned int i;
    int retval;

    chunk = malloc(sizeof(*chunk));
    if (!chunk) return ENOMEM;

    retval = backing_alloc(pool->chunk_len, &base, &dma_base);
    if (retval) {
        free(chunk);
        return retval;
    }

    if (pool->dma_mask < pool->chunk_len - 1 ||
        dma_base > pool->dma_mask - (pool->chunk_len - 1)) {
        munmap(base, pool->chunk_len);
        free(chunk);
        return ERANGE;
    }

    max_slots = pool->chunk_len / pool->stride;
    if (max_slots > (size_t)-1 / sizeof(*chunk->slots)) {
        munmap(base, pool->chunk_len);
        free(chunk);
        return EOVERFLOW;
    }

    chunk->slots = calloc(max_slots, sizeof(*chunk->slots));
    if (!chunk->slots) {
        munmap(base, pool->chunk_len);
        free(chunk);
        return ENOMEM;
    }

    chunk->num_slots = 0;
    off = 0;
    while (off <= pool->chunk_len - pool->stride) {
        phys_bytes dma;

        if (!align_up_sz(off, pool->align, &off)) break;
        if (off > pool->chunk_len - pool->stride) break;

        dma = dma_base + off;

        if (pool->boundary && dma / pool->boundary !=
                                   (dma + pool->size - 1) / pool->boundary) {
            off = (size_t)((dma / pool->boundary + 1) * pool->boundary -
                           dma_base);
            continue;
        }

        slot = &chunk->slots[chunk->num_slots++];
        slot->cpu = (char*)base + off;
        slot->dma = dma;
        off += pool->stride;
    }

    if (chunk->num_slots == 0) {
        munmap(base, pool->chunk_len);
        free(chunk->slots);
        free(chunk);
        return ENOMEM;
    }

    chunk->map_base = base;
    chunk->map_len = pool->chunk_len;
    chunk->dma_base = dma_base;

    chunk->next = pool->chunks;
    pool->chunks = chunk;

    for (i = chunk->num_slots; i > 0; i--) {
        slot = &chunk->slots[i - 1];
        slot->next_free = pool->free_list;
        pool->free_list = slot;
    }

    return 0;
}

int dma_pool_create(const char* name, const struct dma_pool_config* config,
                    struct dma_pool** poolp)
{
    struct dma_pool* pool;
    size_t align;
    size_t stride;
    size_t chunk_len;
    size_t base;
    size_t namelen;

    if (poolp) *poolp = NULL;
    if (!config || !poolp) return EINVAL;

    if (config->size == 0) return EINVAL;

    align = config->align ? config->align : 1;
    if (!is_pow2(align)) return EINVAL;
    if (align > ARCH_PG_SIZE) return EOPNOTSUPP;

    if (config->boundary && !is_pow2(config->boundary)) return EINVAL;

    if (config->dma_mask == 0 ||
        config->dma_mask & (config->dma_mask + 1))
        return EINVAL;

#if !DMA_POOL_BACKEND
    return EOPNOTSUPP;
#endif

    if (!align_up_sz(config->size, align, &stride)) return EOVERFLOW;

    if (config->boundary && config->boundary < stride) return EINVAL;

    base = stride > ARCH_PG_SIZE ? stride : ARCH_PG_SIZE;
    if (config->boundary) {
        if (base > (size_t)-1 - (config->boundary - 1)) return EOVERFLOW;
        base += config->boundary - 1;
    }
    if (!align_up_sz(base, ARCH_PG_SIZE, &chunk_len)) return EOVERFLOW;

    pool = calloc(1, sizeof(*pool));
    if (!pool) return ENOMEM;

    if (name) {
        namelen = strlen(name) + 1;
        pool->name = malloc(namelen);
        if (!pool->name) {
            free(pool);
            return ENOMEM;
        }
        memcpy(pool->name, name, namelen);
    }

    pool->size = config->size;
    pool->align = align;
    pool->boundary = config->boundary;
    pool->dma_mask = config->dma_mask;
    pool->stride = stride;
    pool->chunk_len = chunk_len;

    *poolp = pool;
    return 0;
}

void* dma_pool_alloc(struct dma_pool* pool, unsigned int flags,
                     phys_bytes* dmap)
{
    struct dma_pool_slot* slot;

    if (dmap) *dmap = 0;
    if (!pool || !dmap) return NULL;
    if (flags & ~(unsigned int)DMA_POOL_NOGROW) return NULL;

    slot = pool->free_list;
    if (!slot) {
        if (flags & DMA_POOL_NOGROW) return NULL;

        if (dma_pool_grow(pool)) return NULL;
        slot = pool->free_list;
    }

    pool->free_list = slot->next_free;
    slot->next_free = NULL;
    slot->in_use = 1;
    pool->active++;

    *dmap = slot->dma;
    return slot->cpu;
}

void* dma_pool_zalloc(struct dma_pool* pool, unsigned int flags,
                      phys_bytes* dmap)
{
    void* cpu = dma_pool_alloc(pool, flags, dmap);

    if (cpu) memset(cpu, 0, pool->size);

    return cpu;
}

int dma_pool_free(struct dma_pool* pool, void* cpu, phys_bytes dma)
{
    struct dma_pool_chunk* chunk;
    unsigned int i;

    if (!pool || !cpu) return EINVAL;

    for (chunk = pool->chunks; chunk; chunk = chunk->next) {
        if ((char*)cpu >= (char*)chunk->map_base &&
            (char*)cpu < (char*)chunk->map_base + chunk->map_len)
            break;
    }
    if (!chunk) return EINVAL;

    for (i = 0; i < chunk->num_slots; i++) {
        struct dma_pool_slot* slot = &chunk->slots[i];

        if (slot->cpu == cpu) {
            if (!slot->in_use || slot->dma != dma) return EINVAL;

            slot->in_use = 0;
            pool->active--;
            slot->next_free = pool->free_list;
            pool->free_list = slot;
            return 0;
        }
    }

    return EINVAL;
}

int dma_pool_destroy(struct dma_pool* pool)
{
    struct dma_pool_chunk* chunk;

    if (!pool) return 0;
    if (pool->active != 0) return EBUSY;

    chunk = pool->chunks;
    while (chunk) {
        struct dma_pool_chunk* next = chunk->next;

        munmap(chunk->map_base, chunk->map_len);
        free(chunk->slots);
        free(chunk);
        chunk = next;
    }

    free(pool->name);
    free(pool);
    return 0;
}
