
unsigned int __udivsi3(unsigned int n, unsigned int d);

unsigned int __umodsi3(unsigned int a, unsigned int b)
{
    return a - __udivsi3(a, b) * b;
}
