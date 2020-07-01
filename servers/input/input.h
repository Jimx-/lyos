#ifndef _INPUT_INPUT_H_
#define _INPUT_INPUT_H_

#include <sys/types.h>
#include <lyos/type.h>

#define INPUT_DEV_MAX 64

struct input_dev {
    dev_t minor;
    endpoint_t owner;
};

#endif
