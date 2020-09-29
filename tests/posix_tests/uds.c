#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <string.h>
#include <poll.h>

#include "munit/munit.h"

static void run_client(void) {}

static MunitResult run_server(void)
{
    int listen_fd;

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    munit_assert_int(listen_fd, >=, 0);

    return MUNIT_OK;
}

static MunitResult test_uds_read_write(const MunitParameter params[],
                                       void* data)
{
    pid_t cpid;
    MunitResult res;

    cpid = fork();
    munit_assert_int(cpid, >=, 0);

    if (cpid == 0) {
        run_client();
        _exit(0);
    } else {
        res = run_server();
        wait(NULL);
    }

    return res;
}

MunitTest uds_tests[] = {
    {(char*)"/read-write", test_uds_read_write, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
