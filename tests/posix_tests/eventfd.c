#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <string.h>
#include <poll.h>

#include "munit/munit.h"

static MunitResult test_eventfd_read_write(const MunitParameter params[],
                                           void* data)
{
    int fd;
    uint64_t wvals[] = {1, 2, 4, 7, 14};
    uint64_t rval, wsum;
    int i, n;

    wsum = 0;
    for (i = 0; i < sizeof(wvals) / sizeof(wvals[0]); i++) {
        wsum += wvals[i];
    }

    fd = eventfd(0, 0);
    munit_assert_int(fd, >=, 0);

    for (i = 0; i < sizeof(wvals) / sizeof(wvals[0]); i++) {
        n = write(fd, &wvals[i], sizeof(uint64_t));
        munit_assert_int(n, ==, sizeof(uint64_t));
    }

    n = read(fd, &rval, sizeof(uint64_t));
    munit_assert_int(n, ==, sizeof(uint64_t));
    munit_assert_uint64(rval, ==, wsum);

    close(fd);

    return MUNIT_OK;
}

static MunitResult
test_eventfd_read_write_semaphore(const MunitParameter params[], void* data)
{
    int fd;
    uint64_t wvals = 100;
    uint64_t rval;
    int n;

    fd = eventfd(0, EFD_SEMAPHORE);
    munit_assert_int(fd, >=, 0);

    n = write(fd, &wvals, sizeof(uint64_t));
    munit_assert_int(n, ==, sizeof(uint64_t));

    n = read(fd, &rval, sizeof(uint64_t));
    munit_assert_int(n, ==, sizeof(uint64_t));
    munit_assert_uint64(rval, ==, 1);

    close(fd);

    return MUNIT_OK;
}

static MunitResult test_eventfd_poll(const MunitParameter params[], void* data)
{
    int fd;
    uint64_t wvals = 100;
    uint64_t rval;
    int retval, n;

    fd = eventfd(0, 0);
    munit_assert_int(fd, >=, 0);

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT;
    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 1);
    munit_assert_false(pfd.revents & POLLIN);
    munit_assert(pfd.revents & POLLOUT);

    n = write(fd, &wvals, sizeof(uint64_t));
    munit_assert_int(n, ==, sizeof(uint64_t));

    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 1);
    munit_assert(pfd.revents & POLLIN);
    munit_assert(pfd.revents & POLLOUT);

    close(fd);

    return MUNIT_OK;
}

MunitTest eventfd_tests[] = {
    {(char*)"/read-write", test_eventfd_read_write, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/read-write-semaphore", test_eventfd_read_write_semaphore, NULL,
     NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/poll", test_eventfd_poll, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
