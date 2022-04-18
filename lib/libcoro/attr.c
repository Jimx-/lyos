#include <errno.h>
#include <libcoro/libcoro.h>


int coro_attr_init(coro_attr_t* attr)
{
    attr->stackaddr = NULL;
    attr->stacksize = 0;

    return 0;
}

int coro_attr_setstacksize(coro_attr_t* attr, size_t stacksize)
{
    if (stacksize == 0) {
        return EINVAL;
    }

    attr->stacksize = stacksize;

    return 0;
}

int coro_attr_destroy(coro_attr_t* attr) { return 0; }
