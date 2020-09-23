#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <poll.h>
#include <lyos/limits.h>

#include "munit/munit.h"

static MunitResult test_pipe_read_write(const MunitParameter params[],
                                        void* data)
{
    int fds[2];
    pid_t cpid;
    static char wbuf[2 * PIPE_BUF];
    static char rbuf[2 * PIPE_BUF];
    off_t roff = 0;
    size_t rsize;
    int i;

    memset(rbuf, 0, sizeof(rbuf));
    for (i = 0; i < sizeof(wbuf); i++) {
        wbuf[i] = i & 0xff;
    }

    pipe(fds);

    cpid = fork();
    munit_assert_int(cpid, >=, 0);

    if (cpid == 0) {
        close(fds[0]);
        write(fds[1], wbuf, sizeof(wbuf));
        close(fds[1]);

        _exit(0);
    } else {
        close(fds[1]);

        while ((rsize = read(fds[0], rbuf, sizeof(rbuf))) > 0) {
            for (i = 0; i < rsize; i++) {
                munit_assert_char(wbuf[i + roff], ==, rbuf[i]);
            }
            roff += rsize;
        }

        munit_assert_int(roff, ==, sizeof(wbuf));

        close(fds[0]);
        wait(NULL);
    }

    return MUNIT_OK;
}

static MunitResult test_pipe_close_reader(const MunitParameter params[],
                                          void* data)
{
    int fds[2];
    int retval;

    retval = pipe(fds);
    munit_assert_int(retval, ==, 0);

    close(fds[0]);

    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fds[1];

    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 1);
    munit_assert_false(pfd.revents & POLLOUT);
    munit_assert(pfd.revents & POLLERR);
    munit_assert_false(pfd.revents & POLLHUP);

    close(fds[1]);

    return MUNIT_OK;
}

static MunitResult test_pipe_close_writer(const MunitParameter params[],
                                          void* data)
{
    int fds[2];
    int retval;

    retval = pipe(fds);
    munit_assert_int(retval, ==, 0);

    close(fds[1]);

    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fds[0];

    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 1);
    munit_assert_false(pfd.revents & POLLIN);
    munit_assert_false(pfd.revents & POLLERR);
    munit_assert(pfd.revents & POLLHUP);

    close(fds[0]);

    return MUNIT_OK;
}

MunitTest pipe_tests[] = {
    {(char*)"/read-write", test_pipe_read_write, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/close-reader", test_pipe_close_reader, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/close-writer", test_pipe_close_writer, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
