#include <errno.h>

int clone(int (*fn)(void* arg), void* child_stack, int flags, void* arg, ...)
{
    return -ENOSYS;
}
