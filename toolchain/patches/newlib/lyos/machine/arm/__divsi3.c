
int __udivsi3(int n, int d);

int __divsi3(int n, int d)
{
    int minus = 0;
    if (n < 0) {
        n = -n;
        minus++;
    }
    if (d < 0) {
        d = -d;
        minus++;
    }

    int q = __udivsi3(n, d);

    if (minus == 1) q = -q;

    return q;
}

