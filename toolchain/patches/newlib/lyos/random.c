#include <sys/types.h>
#include <sys/random.h>

ssize_t getrandom(void* buffer, size_t max_size, unsigned int flags)
{
    return max_size;
}
