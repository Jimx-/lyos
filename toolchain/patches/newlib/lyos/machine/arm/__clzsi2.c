int __clzsi2(int a)
{
    int x = (int)a;
    int t = ((x & 0xFFFF0000) == 0) << 4;  /* if (x is small) t = 16 else 0 */
    x >>= 16 - t;      /* x = [0 - 0xFFFF] */
    int r = t;       /* r = [0, 16] */
    /* return r + clz(x) */
    t = ((x & 0xFF00) == 0) << 3;
    x >>= 8 - t;       /* x = [0 - 0xFF] */
    r += t;            /* r = [0, 8, 16, 24] */
    /* return r + clz(x) */
    t = ((x & 0xF0) == 0) << 2;
    x >>= 4 - t;       /* x = [0 - 0xF] */
    r += t;            /* r = [0, 4, 8, 12, 16, 20, 24, 28] */
    /* return r + clz(x) */
    t = ((x & 0xC) == 0) << 1;
    x >>= 2 - t;       /* x = [0 - 3] */
    r += t;            /* r = [0 - 30] and is even */
    return r + ((2 - x) & -((x & 2) == 0));
}
