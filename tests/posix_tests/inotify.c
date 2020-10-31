#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

#include "munit/munit.h"

static MunitResult test_inotify_create_delete(const MunitParameter params[],
                                              void* data)
{
    char tempdir[] = "/tmp/inotify-test-XXXXXX";
    const char* filename = "test";
    char* tmp;
    int dirfd, inotify_fd, wd, fd;
    struct pollfd pfd;
    char buf[200];
    struct inotify_event* event;
    int retval, n;

    tmp = mkdtemp(tempdir);
    munit_assert_not_null(tmp);

    dirfd = open(tempdir, O_RDONLY | O_DIRECTORY);
    munit_assert_int(dirfd, >=, 0);

    inotify_fd = inotify_init();
    munit_assert_int(inotify_fd, >=, 0);

    wd = inotify_add_watch(inotify_fd, tempdir, IN_CREATE | IN_DELETE);
    munit_assert_int(wd, >=, 0);

    /* no event yet */
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = inotify_fd;
    pfd.events = POLLIN;
    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 0);

    /* create event */
    fd = openat(dirfd, filename, O_WRONLY | O_CREAT, 0666);
    munit_assert_int(fd, >=, 0);

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = inotify_fd;
    pfd.events = POLLIN;
    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 1);
    munit_assert(pfd.revents & POLLIN);

    n = read(inotify_fd, buf, sizeof(buf));
    munit_assert_int(n, >, 0);

    event = (struct inotify_event*)buf;
    munit_assert_int(event->wd, ==, wd);
    munit_assert(event->mask & IN_CREATE);
    munit_assert_string_equal(event->name, filename);

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = inotify_fd;
    pfd.events = POLLIN;
    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 0);

    /* delete event */
    retval = unlinkat(dirfd, filename, 0);
    munit_assert_int(retval, ==, 0);

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = inotify_fd;
    pfd.events = POLLIN;
    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 1);
    munit_assert(pfd.revents & POLLIN);

    n = read(inotify_fd, buf, sizeof(buf));
    munit_assert_int(n, >, 0);

    event = (struct inotify_event*)buf;
    munit_assert_int(event->wd, ==, wd);
    munit_assert(event->mask & IN_DELETE);
    munit_assert_string_equal(event->name, filename);

    close(inotify_fd);
    close(fd);
    close(dirfd);
    rmdir(tempdir);

    return MUNIT_OK;
}

MunitTest inotify_tests[] = {
    {(char*)"/inotify-create-delete", test_inotify_create_delete, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
