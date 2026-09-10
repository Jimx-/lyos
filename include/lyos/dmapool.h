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

#ifndef _LYOS_DMAPOOL_H_
#define _LYOS_DMAPOOL_H_

/* DMA pool interface modeled after Linux's include/linux/dmapool.h.
 *
 * A pool suballocates fixed-size, aligned, physically contiguous objects
 * with persistent DMA addresses from larger backing allocations, for
 * hardware descriptors, ring segments, device contexts, and small
 * fixed-size control buffers.
 *
 * dma_pool_alloc() and dma_pool_zalloc() follow Linux's interface: they
 * return the CPU address of the object, or NULL on failure, and store the
 * device DMA address through dmap (zeroed on failure).  A DMA address of
 * zero itself is valid; the NULL pointer is the only error indication.
 * dma_pool_create(), dma_pool_free(), and dma_pool_destroy() return zero
 * on success or a positive errno.  Callers must serialize all operations
 * on a pool.  The initial backend requires a coherent identity-DMA
 * platform; dma_pool_create() returns EOPNOTSUPP elsewhere.
 */

#include <stddef.h>
#include <lyos/types.h>

struct dma_pool;

struct dma_pool_config {
    size_t size;         /* caller-visible object bytes; nonzero */
    size_t align;        /* power of two; zero means one */
    size_t boundary;     /* power of two; zero means unrestricted */
    phys_bytes dma_mask; /* inclusive maximum device DMA address */
};

#define DMA_POOL_NOGROW 0x01U

int dma_pool_create(const char* name, const struct dma_pool_config* config,
                    struct dma_pool** poolp);
void* dma_pool_alloc(struct dma_pool* pool, unsigned int flags,
                     phys_bytes* dmap);
void* dma_pool_zalloc(struct dma_pool* pool, unsigned int flags,
                      phys_bytes* dmap);
int dma_pool_free(struct dma_pool* pool, void* cpu, phys_bytes dma);
int dma_pool_destroy(struct dma_pool* pool);

#endif
