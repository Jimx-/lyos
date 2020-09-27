#ifndef _UDS_UDS_H_
#define _UDS_UDS_H_

#include <libsockdriver/libsockdriver.h>

struct udssock {
    struct sockdriver_sock sock;
    struct udssock* conn;
};

#endif
