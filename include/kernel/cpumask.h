#ifndef _CPUMASK_H_
#define _CPUMASK_H_

#include <lyos/config.h>
#include <lyos/bitmap.h>

#define NR_CPUMASK_BITS CONFIG_SMP_MAX_CPUS

typedef struct cpumask {
    bitchunk_t bits[BITCHUNKS(CONFIG_SMP_MAX_CPUS)];
} cpumask_t;

#define cpumask_bits(maskp) ((maskp)->bits)

#define to_cpumask(bitmap) ((struct cpumask*)(bitmap))

static inline int cpumask_test_cpu(const struct cpumask* cpumask, int cpu)
{
    return !!GET_BIT(cpumask_bits(cpumask), cpu);
}

static inline int cpumask_equal(const struct cpumask* mask1,
                                const struct cpumask* mask2)
{
    return bitmap_equal(cpumask_bits(mask1), cpumask_bits(mask2),
                        NR_CPUMASK_BITS);
}

extern const bitchunk_t cpu_bit_bitmap[BITCHUNK_BITS + 1]
                                      [BITCHUNKS(CONFIG_SMP_MAX_CPUS)];

static inline const struct cpumask* get_cpu_mask(unsigned int cpu)
{
    const bitchunk_t* p = cpu_bit_bitmap[1 + cpu % BITCHUNK_BITS];
    p -= cpu / BITCHUNK_BITS;
    return to_cpumask(p);
}

#define cpumask_of(cpu) (get_cpu_mask(cpu))

#endif
