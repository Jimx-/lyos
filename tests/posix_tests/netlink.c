#define _GNU_SOURCE
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <lyos/netlink.h>
#include <string.h>
#include <poll.h>
#include <sys/uio.h>

#include "munit/munit.h"

static MunitResult test_nl_uevent(const MunitParameter params[], void* data)
{
    int sock_fd;
    struct sockaddr_nl addr;
    int retval;

    sock_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    munit_assert_int(sock_fd, >=, 0);

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    retval = bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr));
    munit_assert_int(retval, ==, 0);

    close(sock_fd);
    return MUNIT_OK;
}

MunitTest netlink_tests[] = {
    {(char*)"/uevent", test_nl_uevent, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
