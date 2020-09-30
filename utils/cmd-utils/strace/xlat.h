#ifndef _STRACE_XLAT_H_
#define _STRACE_XLAT_H_

#include <sys/types.h>
#include <stdint.h>

struct xlat_data {
    uint64_t val;
    const char* str;
};

struct xlat {
    const struct xlat_data* data;
    size_t size;
};

#define XLAT(val)             \
    {                         \
        (unsigned)(val), #val \
    }

#define XLAT_SIZE(data) (sizeof(data) / sizeof((data)[0]))

#define DEFINE_XLAT(name) \
    struct xlat name = {.data = name##_data, .size = XLAT_SIZE(name##_data)};

#endif
