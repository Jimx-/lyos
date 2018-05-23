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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include "stddef.h"
#include "errno.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "const.h"
#include "page.h"
#include "region.h"
#include "proto.h"
#include "global.h"

PRIVATE struct hole hole[NR_HOLES]; /* the hole table */
PRIVATE struct hole *hole_head;	/* pointer to first hole */
PRIVATE struct hole *free_slots;/* ptr to list of unused table slots */

PRIVATE void delete_slot(struct hole *prev_ptr, struct hole *hp);
PRIVATE void merge_hole(struct hole * hp);

PUBLIC void mem_init(int mem_start, int free_mem_size)
{
	struct hole *hp;

  	/* Put all holes on the free list. */
  	for (hp = &hole[0]; hp < &hole[NR_HOLES]; hp++) {
		hp->h_next = hp + 1;
		hp->h_base = hp->h_len = 0;
  	}
  	hole[NR_HOLES-1].h_next = NULL;
  	hole_head = NULL;
  	free_slots = &hole[0];

	/* Free memory */
	free_mem(mem_start, free_mem_size);
}

/*****************************************************************************
 *                                alloc_mem
 *****************************************************************************/
/**
 * Allocate a memory block for a proc.
 *
 * @param pid  Which proc the memory is for.
 * @param memsize  How many bytes is needed.
 *
 * @return  The base of the memory just allocated.
 *****************************************************************************/
PUBLIC int alloc_mem(int memsize)
{
 	struct hole *hp, *prev_ptr;
	int old_base;

    prev_ptr = NULL;
	hp = hole_head;
	
	while (hp != NULL) {
		if (hp->h_len >= memsize) {
			/* We found a hole that is big enough.  Use it. */
			old_base = hp->h_base;
			hp->h_base += memsize;
			hp->h_len -= memsize;

			/* Delete the hole if used up completely. */
			if (hp->h_len == 0) delete_slot(prev_ptr, hp);

			/* Update stat */
			mem_info.mem_free -= memsize;

			/* Return the start address of the acquired block. */
			return(old_base);
		}

		prev_ptr = hp;
		hp = hp->h_next;
	}
	printl("MM: alloc_mem() failed.(Out of memory)\n");
  	return(-ENOMEM);
}

/**
 * Allocate physical pages.
 * @param  nr_pages How many pages are needed.
 * @return          Ptr to the memory.
 */
PUBLIC int alloc_pages(int nr_pages, int memflags)
{
	int memsize = nr_pages * PG_SIZE;
 	struct hole *hp, *prev_ptr;
	int old_base;
	phys_bytes page_align = PAGE_ALIGN;

	if (memflags & APF_ALIGN16K) {
		page_align = 0x4000;
	}

    prev_ptr = NULL;
	hp = hole_head;
	while (hp != NULL) {
		int alignment = 0;
		if (hp->h_base % page_align != 0)
			alignment = page_align - (hp->h_base % page_align);
		if (hp->h_len >= memsize + alignment) {
			/* We found a hole that is big enough.  Use it. */
			old_base = hp->h_base + alignment;
			hp->h_base += memsize + alignment;
			hp->h_len -= (memsize + alignment);
			if (prev_ptr && prev_ptr->h_base + prev_ptr->h_len == old_base)
				prev_ptr->h_len += alignment;

			mem_info.mem_free -= memsize;

			/* Delete the hole if used up completely. */
			if (hp->h_len == 0) delete_slot(prev_ptr, hp);

			/* Return the start address of the acquired block. */
			return(old_base);
		}

		prev_ptr = hp;
		hp = hp->h_next;
	}
	printl("MM: alloc_pages() failed.(Out of memory)\n");
  	return(-ENOMEM);
}

/*****************************************************************************
 *                                free_mem
 *****************************************************************************/
/**
 * Free a memory block.
 *
 * @param base	Base address of block to free.
 * @param len	Number of bytes to free.
 *
 * @return  Zero if success.
 *****************************************************************************/
PUBLIC int free_mem(int base, int len)
{
	struct hole *hp, *new_ptr, *prev_ptr;

	if (len == 0) return EINVAL;
  	if ((new_ptr = free_slots) == NULL)
  		panic("hole table full");
	new_ptr->h_base = base;
	new_ptr->h_len = len;
 	free_slots = new_ptr->h_next;
	hp = hole_head;

	mem_info.mem_free += len;

	/* Insert the slot to a proper place */
	if (hp == NULL || base <= hp->h_base) {
	/* If there's no hole or the block's address is less than the lowest hole,
	   put it on the front of the list */
		new_ptr->h_next = hp;
		hole_head = new_ptr;
		merge_hole(new_ptr);
		return 0;
 	}

	/* Find where it should go */
	prev_ptr = NULL;
	while (hp != NULL && base > hp->h_base) {
		prev_ptr = hp;
		hp = hp->h_next;
  	}

  	new_ptr->h_next = prev_ptr->h_next;
  	prev_ptr->h_next = new_ptr;
	merge_hole(prev_ptr);
	return 0;
}

/*******************************************************************
 *			delete_hole
 *******************************************************************/
/**
 * Remove an entry from the list.
 *
 *******************************************************************/
PRIVATE void delete_slot(struct hole *prev_ptr, struct hole *hp)
{
	if (hp == hole_head)
		hole_head = hp->h_next;
	else
		prev_ptr->h_next = hp->h_next;

  	hp->h_next = free_slots;
  	hp->h_base = hp->h_len = 0;
  	free_slots = hp;
}

/*******************************************************************
 *			merge_hole
 *******************************************************************/
/**
 * Merge contiguous holes.
 *
 *******************************************************************/
PRIVATE void merge_hole(struct hole * hp)
{
	struct hole *next_ptr;

  	if ((next_ptr = hp->h_next) == NULL) return; /* last hole */
  	if (hp->h_base + hp->h_len == next_ptr->h_base) {
		hp->h_len += next_ptr->h_len;	/* first one gets second one's mem */
		delete_slot(hp, next_ptr);
  	} else {
		hp = next_ptr;
  	}

  	if ((next_ptr = hp->h_next) == NULL) return;	/* hp is the last hole now */
  	if (hp->h_base + hp->h_len == next_ptr->h_base) {
		hp->h_len += next_ptr->h_len;
		delete_slot(hp, next_ptr);
  	}
}
