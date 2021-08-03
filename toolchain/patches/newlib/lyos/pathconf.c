#include <errno.h>
#include <unistd.h>
#include <sys/syslimits.h>

long pathconf(const char* path, int name)
{
    switch (name) {
    case _PC_NAME_MAX:
        return NAME_MAX;
    case _PC_PATH_MAX:
        return PATH_MAX;
    case _PC_LINK_MAX:
        return LINK_MAX;
    case _PC_PIPE_BUF:
        return PIPE_BUF;
    }

    errno = EINVAL;
    return -1;
}

long fpathconf(int fd, int name)
{
    switch (name) {
    case _PC_NAME_MAX:
        return NAME_MAX;
    case _PC_PATH_MAX:
        return PATH_MAX;
    case _PC_LINK_MAX:
        return LINK_MAX;
    case _PC_PIPE_BUF:
        return PIPE_BUF;
    }

    errno = EINVAL;
    return -1;
}
