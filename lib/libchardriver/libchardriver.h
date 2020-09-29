#ifndef _LIBCHARDRIVER_H_
#define _LIBCHARDRIVER_H_

#include <lyos/types.h>

typedef unsigned int cdev_id_t;

struct chardriver {
    int (*cdr_open)(dev_t minor, int access);
    int (*cdr_close)(dev_t minor);
    ssize_t (*cdr_read)(dev_t minor, u64 pos, endpoint_t endpoint,
                        mgrant_id_t grant, unsigned int count, cdev_id_t id);
    ssize_t (*cdr_write)(dev_t minor, u64 pos, endpoint_t endpoint,
                         mgrant_id_t grant, unsigned int count, cdev_id_t id);
    int (*cdr_ioctl)(dev_t minor, int request, endpoint_t endpoint,
                     mgrant_id_t grant, endpoint_t user_endpoint, cdev_id_t id);
    int (*cdr_mmap)(dev_t minor, endpoint_t endpoint, char* addr, off_t offset,
                    size_t length, char** retaddr);
    int (*cdr_select)(dev_t minor, int ops, endpoint_t endpoint);
    void (*cdr_intr)(unsigned mask);
    void (*cdr_alarm)(clock_t timestamp);
    void (*cdr_other)(MESSAGE* msg);
};

void chardriver_process(struct chardriver* cd, MESSAGE* msg);
int chardriver_task(struct chardriver* cd);
void chardriver_reply(MESSAGE* msg, int retval);
void chardriver_reply_io(endpoint_t endpoint, cdev_id_t id, int retval);
void chardriver_poll_notify(dev_t minor, __poll_t ops);
int chardriver_get_minor(MESSAGE* msg, dev_t* minor);

#endif
