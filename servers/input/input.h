#ifndef _INPUT_INPUT_H_
#define _INPUT_INPUT_H_

#include <sys/types.h>
#include <lyos/types.h>

struct input_handle;

struct input_dev {
    struct list_head node;
    struct list_head h_list;

    input_dev_id_t id;
    endpoint_t owner;

    device_id_t dev_id;
    device_id_t input_dev_id;
};

struct input_handler {
    struct list_head node;
    struct list_head h_list;

    const char* name;
    dev_t minor;
    void* private;

    void (*event)(struct input_handle* handler, unsigned int type,
                  unsigned int code, int value);
    void (*events)(struct input_handle* handler, const struct input_value* vals,
                   size_t count);
    int (*connect)(struct input_handler* handler, struct input_dev* dev);
};

struct input_handle_ops {
    int (*open)(struct input_handle* handle);
    int (*close)(struct input_handle* handle);
    ssize_t (*read)(struct input_handle* handle, endpoint_t endpoint, char* buf,
                    unsigned int count, cdev_id_t id);
};

struct input_handle {
    struct list_head node;

    dev_t minor;
    int open;
    struct input_dev* dev;
    struct input_handler* handler;

    struct input_handle_ops* ops;
    void* private;

    struct list_head d_node;
    struct list_head h_node;
};

extern class_id_t input_class;

/* input/main.c */
int input_register_handler(struct input_handler* handler);
int input_open_device(struct input_handle* handle);
void input_close_device(struct input_handle* handle);
int input_register_handle(struct input_handle* handle);

/* input/evdev.c */
int evdev_init(void);

#endif
