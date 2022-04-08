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

#ifndef _ARCH_PAGE_H_
#define _ARCH_PAGE_H_

#include <lyos/config.h>
#include <lyos/const.h>

#define VA_BITS (CONFIG_ARM64_VA_BITS)

#define KERNEL_VMA (_PAGE_END(VA_BITS_MIN))

#if VA_BITS > 48
#define VA_BITS_MIN (48)
#else
#define VA_BITS_MIN (VA_BITS)
#endif

#define _PAGE_END(va) (-(_AC(1, UL) << ((va)-1)))

#define ARM64_PG_SHIFT CONFIG_ARM64_PAGE_SHIFT
#define ARM64_PG_SIZE  (_AC(1, UL) << ARM64_PG_SHIFT)
#define ARM64_PG_MASK  (~(ARM64_PG_SIZE - 1))

#define ARCH_PG_SIZE  ARM64_PG_SIZE
#define ARCH_PG_SHIFT ARM64_PG_SHIFT
#define ARCH_PG_MASK  ARM64_PG_MASK

#endif
