#include <sys/types.h>
#include <stdint.h>
#include <sys/random.h>

// Borrowed from musl
static uint32_t init[] = {
    0x00000000, 0x5851f42d, 0xc0b18ccf, 0xcbb5f646, 0xc7033129, 0x30705b04,
    0x20fd5db4, 0x9a8b7f78, 0x502959d8, 0xab894868, 0x6c0356a7, 0x88cdb7ff,
    0xb477d43f, 0x70a3a52b, 0xa8e4baf1, 0xfd8341fc, 0x8ae16fd9, 0x742d2f7a,
    0x0d1f0796, 0x76035e09, 0x40f7702c, 0x6fa72ca5, 0xaaa84157, 0x58a0df74,
    0xc74a0364, 0xae533cc4, 0x04185faf, 0x6de3b115, 0x0cab8628, 0xf043bfa4,
    0x398150e9, 0x37521657};

static int n = 31;
static int i = 3;
static int j = 0;
static uint32_t* x = init + 1;

static uint32_t lcg31(uint32_t x)
{
    return (1103515245 * x + 12345) & 0x7fffffff;
}

static uint64_t lcg64(uint64_t x) { return 6364136223846793005ULL * x + 1; }

static void* savestate(void)
{
    x[-1] = (n << 16) | (i << 8) | j;
    return x - 1;
}

static void loadstate(uint32_t* state)
{
    x = state + 1;
    n = x[-1] >> 16;
    i = (x[-1] >> 8) & 0xff;
    j = x[-1] & 0xff;
}

long random(void)
{
    long k;

    if (n == 0) {
        k = x[0] = lcg31(x[0]);
        return k;
    }
    x[i] += x[j];
    k = x[i] >> 1;
    if (++i == n) i = 0;
    if (++j == n) j = 0;

    return k;
}

void srandom(unsigned int seed)
{
    int k;
    uint64_t s = seed;

    if (n == 0) {
        x[0] = s;
        return;
    }
    i = n == 31 || n == 7 ? 3 : 1;
    j = 0;
    for (k = 0; k < n; k++) {
        s = lcg64(s);
        x[k] = s >> 32;
    }

    x[0] |= 1;
}

ssize_t getrandom(void* buffer, size_t max_size, unsigned int flags)
{
    return max_size;
}
