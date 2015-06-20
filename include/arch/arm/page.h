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

#ifndef _PAGE_H_
#define _PAGE_H_

typedef unsigned int    pde_t;
typedef unsigned int    pte_t;

#define ARM_VM_DIR_ENTRIES      4096

#define ARM_SECTION_SIZE        (1024 * 1024) /* 1 MB section */
/* Big page (1MB section) specific flags. */
#define ARM_VM_SECTION          (1 << 1)  /* 1MB section */
#define ARM_VM_SECTION_PRESENT      (1 << 1)  /* Section is present */
#define ARM_VM_SECTION_B        (1 << 2)  /* B Bit */
#define ARM_VM_SECTION_C        (1 << 3)  /* C Bit */
#define ARM_VM_SECTION_DOMAIN       (0xF << 5) /* Domain Number */
#define ARM_VM_SECTION_SUPER        (0x1 << 10) /* Super access only AP[1:0] */
#define ARM_VM_SECTION_USER     (0x3 << 10) /* Super/User access AP[1:0] */
#define ARM_VM_SECTION_TEX0     (1 << 12) /* TEX[0] */
#define ARM_VM_SECTION_TEX1     (1 << 13) /* TEX[1] */
#define ARM_VM_SECTION_TEX2     (1 << 14) /* TEX[2] */
#define ARM_VM_SECTION_RO       (1 << 15)   /* Read only access AP[2] */
#define ARM_VM_SECTION_SHAREABLE    (1 << 16)  /* Shareable */
#define ARM_VM_SECTION_NOTGLOBAL    (1 << 17)  /* Not Global */
#define ARM_VM_SECTION_WTWB (ARM_VM_SECTION_TEX2 | ARM_VM_SECTION_TEX0 | ARM_VM_SECTION_C)
#define ARM_VM_SECTION_CACHED ARM_VM_SECTION_WTWB

#define ARM_VM_SECTION_SHIFT    20
#define ARM_VM_SECTION_MASK     (~((1 << ARM_VM_SECTION_SHIFT) - 1))

#define PG_SIZE                 0x1000      /* 4kB */

#define ARCH_VM_DIR_ENTRIES      ARM_VM_DIR_ENTRIES

#define ARCH_PG_SIZE            PG_SIZE

#define ARCH_PDE(x)     ((unsigned long)(x) / ARM_SECTION_SIZE)

#endif
