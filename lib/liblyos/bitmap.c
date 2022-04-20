#include <lyos/types.h>
#include <lyos/bitmap.h>

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
