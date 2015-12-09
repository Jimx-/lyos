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

#ifndef _MM_CONST_H_
#define _MM_CONST_H_
    
/* stack guard */
/**
 * when there is a page fault caused by access to this guard area,
 * stack size will be enlarged.
 */
#define GROWSDOWN_GUARD_LEN     0x8000

/* Region flags */
#define RF_NORMAL   0x0
#define RF_WRITABLE 0x1
#define RF_SHARED   0x2
#define RF_MAPPED   0x4
#define RF_GUARD    0x8
#define RF_GROWSDOWN 0x10
#define RF_FILEMAP  0x20
#define RF_PRIVATE  0x40

/* Page frame flags */
#define PFF_WRITABLE 0x1
#define PFF_SHARED   0x2
#define PFF_MAPPED   0x4

/* Page allocation types */
#define PGT_SLAB        1
#define PGT_PAGEDIR     2
#define PGT_PAGETABLE   3

/* alloc_page() flags */
#define APF_NORMAL      0x0
#define APF_ALIGN4K     0x1
#define APF_ALIGN16K    0x2

#define MAX_PAGEDIR_PDES    5
 
#endif /* _MM_CONST_H_ */
