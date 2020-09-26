#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <string.h>
#include <poll.h>
#include <sys/epoll.h>
#include <lyos/limits.h>

#include "munit/munit.h"

static MunitResult test_epoll_poll_eventfd_lt(const MunitParameter params[],
                                              void* data)
{
    int fd, epfd;
    struct epoll_event event;
    int retval, n, i;
    uint64_t count;

    fd = eventfd(0, 0);
    munit_assert_int(fd, >=, 0);

    epfd = epoll_create1(0);
    munit_assert_int(epfd, >=, 0);

    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    retval = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    munit_assert_int(retval, ==, 0);

    // no events pending
    memset(&event, 0, sizeof(event));
    n = epoll_wait(epfd, &event, 1, 0);
    munit_assert_int(n, ==, 0);

    count = 1;
    n = write(fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    for (i = 0; i < 2; i++) {
        memset(&event, 0, sizeof(event));
        n = epoll_wait(epfd, &event, 1, 0);
        munit_assert_int(n, ==, 1);
        munit_assert(event.events & EPOLLIN);
    }

    n = read(fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));
    munit_assert_uint64(count, ==, 1);

    memset(&event, 0, sizeof(event));
    n = epoll_wait(epfd, &event, 1, 0);
    munit_assert_int(n, ==, 0);

    close(epfd);
    close(fd);

    return MUNIT_OK;
}

static MunitResult test_epoll_poll_eventfd_et(const MunitParameter params[],
                                              void* data)
{
    int fd, epfd;
    struct epoll_event event;
    int retval, n;
    uint64_t count;

    fd = eventfd(0, 0);
    munit_assert_int(fd, >=, 0);

    epfd = epoll_create1(0);
    munit_assert_int(epfd, >=, 0);

    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN | EPOLLET;
    retval = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    munit_assert_int(retval, ==, 0);

    // no events pending
    memset(&event, 0, sizeof(event));
    n = epoll_wait(epfd, &event, 1, 0);
    munit_assert_int(n, ==, 0);

    count = 1;
    n = write(fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    memset(&event, 0, sizeof(event));
    n = epoll_wait(epfd, &event, 1, 0);
    munit_assert_int(n, ==, 1);
    munit_assert(event.events & EPOLLIN);

    /* reset by EPOLLET */
    memset(&event, 0, sizeof(event));
    n = epoll_wait(epfd, &event, 1, 0);
    munit_assert_int(n, ==, 0);

    close(epfd);
    close(fd);

    return MUNIT_OK;
}

MunitTest epoll_tests[] = {
    {(char*)"/poll-eventfd-lt", test_epoll_poll_eventfd_lt, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/poll-eventfd-et", test_epoll_poll_eventfd_et, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
