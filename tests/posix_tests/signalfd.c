#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <poll.h>
#include <sys/signalfd.h>

#include "munit/munit.h"

static MunitResult test_signalfd_read_signal(const MunitParameter params[],
                                             void* data)
{
    int fd;
    sigset_t mask;
    struct signalfd_siginfo fdsi;
    int retval;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);

    retval = sigprocmask(SIG_BLOCK, &mask, NULL);
    munit_assert_int(retval, ==, 0);

    fd = signalfd(-1, &mask, 0);
    munit_assert_int(fd, >=, 0);

    raise(SIGINT);

    retval = read(fd, &fdsi, sizeof(fdsi));
    munit_assert_int(retval, ==, sizeof(fdsi));
    munit_assert_int(fdsi.ssi_signo, ==, SIGINT);

    close(fd);

    return MUNIT_OK;
}

static MunitResult test_signalfd_poll(const MunitParameter params[], void* data)
{
    int fd;
    sigset_t mask;
    struct signalfd_siginfo fdsi;
    int retval;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);

    retval = sigprocmask(SIG_BLOCK, &mask, NULL);
    munit_assert_int(retval, ==, 0);

    fd = signalfd(-1, &mask, 0);
    munit_assert_int(fd, >=, 0);

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 0);

    raise(SIGINT);

    retval = poll(&pfd, 1, 0);
    munit_assert_int(retval, ==, 1);
    munit_assert(pfd.revents & POLLIN);

    close(fd);

    return MUNIT_OK;
}

MunitTest signalfd_tests[] = {
    {(char*)"/read-signal", test_signalfd_read_signal, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/poll", test_signalfd_poll, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
