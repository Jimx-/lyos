#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <libevdev-1.0/libevdev/libevdev.h>

int main()
{
    struct libevdev* dev = NULL;
    int fd;
    int rc = 1;

    fd = open("/dev/event0", O_RDONLY | O_NONBLOCK);
    rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
        exit(1);
    }

    do {
        struct input_event ev;
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == 0)
            printf("Event: %s %s %d\n", libevdev_event_type_get_name(ev.type),
                   libevdev_event_code_get_name(ev.type, ev.code), ev.value);
    } while (rc == 1 || rc == 0 || rc == -EAGAIN);

    return 0;
}
