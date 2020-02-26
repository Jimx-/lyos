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

#ifndef _BITMAP_H_
#define _BITMAP_H_

#define CHAR_BITS 8
#define BITCHUNK_BITS (sizeof(bitchunk_t) * CHAR_BITS)
#define BITCHUNKS(bits) (((bits) + BITCHUNK_BITS - 1) / BITCHUNK_BITS)

#define MAP_CHUNK(map, bit) (map)[((bit) / BITCHUNK_BITS)]
#define CHUNK_OFFSET(bit) ((bit) % BITCHUNK_BITS)
#define GET_BIT(map, bit) (MAP_CHUNK(map, bit) & (1 << CHUNK_OFFSET(bit)))
#define SET_BIT(map, bit) (MAP_CHUNK(map, bit) |= (1 << CHUNK_OFFSET(bit)))
#define UNSET_BIT(map, bit) (MAP_CHUNK(map, bit) &= ~(1 << CHUNK_OFFSET(bit)))

#endif
