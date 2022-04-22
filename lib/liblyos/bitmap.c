#include <lyos/types.h>
#include <lyos/bitmap.h>

int bitmap_equal(const unsigned long* bitmap1, const unsigned long* bitmap2,
                 unsigned int bits)
{
    unsigned int k, lim = bits / BITCHUNK_BITS;
    for (k = 0; k < lim; ++k)
        if (bitmap1[k] != bitmap2[k]) return 0;

    if (bits % BITCHUNK_BITS)
        if ((bitmap1[k] ^ bitmap2[k]) & BITMAP_LAST_WORD_MASK(bits)) return 0;

    return 1;
}

unsigned long bitmap_find_next_zero_area(bitchunk_t* map, unsigned long size,
                                         unsigned long start, unsigned int nr)
{
    unsigned long i, j, index, end;

    index = size;

    for (i = start; i < size; i++) {
        if (GET_BIT(map, i)) continue;

        index = i;
        end = index + nr;
        if (end > size) return end;

        i = end;
        for (j = index; j < end; j++) {
            if (GET_BIT(map, j)) {
                i = j;
                break;
            }
        }

        if (i == end) break;

        i = i + 1;
    }

    return index;
}
