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

#ifndef _LYOS_SCATTERLIST_H_
#define _LYOS_SCATTERLIST_H_

/* Scatterlist interface modeled after Linux's include/linux/scatterlist.h.
 *
 * Lyos userspace drivers have no struct page.  Instead of a page pointer,
 * page_link holds a page-aligned userspace virtual address base.  The low
 * two bits keep Linux's marker semantics: SG_CHAIN links to another
 * scatterlist array, SG_END terminates the list.  A separate byte offset
 * locates the data within the base's page.  This is a representation
 * adaptation, not binary compatibility.
 *
 * dma_address holds a physical address obtained from umap-based discovery.
 * Address discovery does not pin pages or establish cache coherence.
 */

#include <lyos/types.h>
#include <string.h>
#include <asm/page.h>

#define SG_CHAIN 0x01UL
#define SG_END   0x02UL

#define SG_PAGE_LINK_MASK (SG_CHAIN | SG_END)

struct scatterlist {
    unsigned long page_link; /* page-aligned userspace VA base | flags */
    unsigned int offset;
    unsigned int length;
    phys_bytes dma_address;
};

struct sg_table {
    struct scatterlist* sgl;
    unsigned int nents;      /* mapped entries */
    unsigned int orig_nents; /* original entries */
};

#define sg_dma_address(sg) ((sg)->dma_address)
#define sg_dma_len(sg)     ((sg)->length)

static inline unsigned int __sg_flags(struct scatterlist* sg)
{
    return sg->page_link & SG_PAGE_LINK_MASK;
}

static inline struct scatterlist* sg_chain_ptr(struct scatterlist* sg)
{
    return (struct scatterlist*)(sg->page_link & ~SG_PAGE_LINK_MASK);
}

static inline int sg_is_chain(struct scatterlist* sg)
{
    return __sg_flags(sg) & SG_CHAIN;
}

static inline int sg_is_last(struct scatterlist* sg)
{
    return __sg_flags(sg) & SG_END;
}

/* Assign a page-aligned userspace VA base, preserving marker bits.
 * Lyos counterpart of Linux's sg_assign_page(). */
static inline void sg_assign_va(struct scatterlist* sg, unsigned long va)
{
    unsigned long page_link = sg->page_link & SG_PAGE_LINK_MASK;

    /* The low bits are stolen by the markers; the base must be aligned. */
    if (va & SG_PAGE_LINK_MASK) return;

    sg->page_link = page_link | va;
}

/* Set an SG entry to point at userspace data given a page-aligned base. */
static inline void sg_set_va(struct scatterlist* sg, unsigned long va,
                             unsigned int len, unsigned int offset)
{
    sg_assign_va(sg, va);
    sg->offset = offset;
    sg->length = len;
}

/* Reconstruct the userspace virtual address of an SG entry. */
static inline void* sg_virt(struct scatterlist* sg)
{
    return (void*)((sg->page_link & ~SG_PAGE_LINK_MASK) + sg->offset);
}

/* Initialize the CPU description of an SG entry.  Does not set the DMA
 * address.  Accepts arbitrary byte alignment of buf. */
static inline void sg_set_buf(struct scatterlist* sg, const void* buf,
                              unsigned int buflen)
{
    unsigned long addr = (unsigned long)buf;

    sg_set_va(sg, addr & ~(unsigned long)(ARCH_PG_SIZE - 1), buflen,
              addr & (ARCH_PG_SIZE - 1));
}

/* Loop over each sg element, following the chain pointer if necessary.
 * Chain entries are metadata and must be skipped by data consumers. */
#define for_each_sg(sglist, sg, nr, __i)                              \
    for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))

/* Loop over each sg element in the given sg_table object. */
#define for_each_sgtable_sg(sgt, sg, i) \
    for_each_sg((sgt)->sgl, sg, (sgt)->orig_nents, i)

/* Loop over each sg element in the given DMA-mapped sg_table object. */
#define for_each_sgtable_dma_sg(sgt, sg, i) \
    for_each_sg((sgt)->sgl, sg, (sgt)->nents, i)

static inline void sg_mark_end(struct scatterlist* sg)
{
    /* Set termination bit, clear potential chain bit */
    sg->page_link |= SG_END;
    sg->page_link &= ~SG_CHAIN;
}

static inline void sg_unmark_end(struct scatterlist* sg)
{
    sg->page_link &= ~SG_END;
}

static inline void __sg_chain(struct scatterlist* chain_sg,
                              struct scatterlist* sgl)
{
    /* offset and length are unused for a chain entry */
    chain_sg->offset = 0;
    chain_sg->length = 0;

    /* Set the lowest bit to indicate a link pointer and clear the
     * termination bit if it happens to be set. */
    chain_sg->page_link = ((unsigned long)sgl | SG_CHAIN) & ~SG_END;
}

/* Chain two sglists together.  Uses the final allocated slot of prv
 * (index prv_nents - 1) as the chain entry; data counts must therefore
 * exclude chain slots. */
static inline void sg_chain(struct scatterlist* prv, unsigned int prv_nents,
                            struct scatterlist* sgl)
{
    __sg_chain(&prv[prv_nents - 1], sgl);
}

static inline struct scatterlist* sg_next(struct scatterlist* sg)
{
    if (sg_is_last(sg)) return NULL;

    sg++;
    if (sg_is_chain(sg)) sg = sg_chain_ptr(sg);

    return sg;
}

/* Mark the final entry of a nonempty table.  Initializing a table with
 * zero entries is not supported; represent an empty list as NULL. */
static inline void sg_init_marker(struct scatterlist* sg, unsigned int nents)
{
    sg_mark_end(&sg[nents - 1]);
}

/* Zero a nonempty table and mark its final entry. */
static inline void sg_init_table(struct scatterlist* sgl, unsigned int nents)
{
    memset(sgl, 0, sizeof(*sgl) * nents);
    sg_init_marker(sgl, nents);
}

/* Initialize a one-entry scatterlist pointing at buf. */
static inline void sg_init_one(struct scatterlist* sg, const void* buf,
                               unsigned int buflen)
{
    sg_init_table(sg, 1);
    sg_set_buf(sg, buf, buflen);
}

#endif
