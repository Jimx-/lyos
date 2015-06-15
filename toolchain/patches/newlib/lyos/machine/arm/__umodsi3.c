
int __udivsi3(int n, int d);

int __umodsi3(int a, int b)
{
    return a - __udivsi3(a, b) * b;
}
