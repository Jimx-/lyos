
int __divsi3(int n, int d);

int __modsi3(int n, int d)
{
    return n - __divsi3(n, d) * d;
}
