#define _GNU_SOURCE
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "munit/munit.h"

static MunitResult test_pty_read_write(const MunitParameter params[],
                                       void* data)
{
    const char* test_string = "Hello world!";
    int master_fd, slave_fd;
    char slave_path[128];
    char buf[128];
    struct stat sbuf;
    int retval, n;
    pid_t pid;

    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    munit_assert_int(master_fd, >=, 0);

    pid = fork();
    munit_assert_int(pid, >=, 0);

    if (pid > 0) {
        n = read(master_fd, buf, sizeof(buf));
        munit_assert_int(n, ==, strlen(test_string));
        buf[n] = '\0';
        munit_assert_string_equal(buf, test_string);

        n = write(master_fd, test_string, strlen(test_string));
        munit_assert_int(n, ==, strlen(test_string));

        close(master_fd);
    } else {
        (void)grantpt(master_fd);
        (void)unlockpt(master_fd);

        retval = ptsname_r(master_fd, slave_path, sizeof(slave_path));
        munit_assert_int(retval, ==, 0);

        close(master_fd);

        slave_fd = open(slave_path, O_RDWR);
        munit_assert_int(slave_fd, >=, 0);

        retval = fstat(slave_fd, &sbuf);
        munit_assert_int(retval, ==, 0);
        munit_assert(S_ISCHR(sbuf.st_mode));

        n = write(slave_fd, test_string, strlen(test_string));
        munit_assert_int(n, ==, strlen(test_string));

        n = read(slave_fd, buf, sizeof(buf));
        munit_assert_int(n, ==, strlen(test_string));
        buf[n] = '\0';
        munit_assert_string_equal(buf, test_string);

        close(slave_fd);
    }

    return MUNIT_OK;
}

MunitTest pty_tests[] = {
    {(char*)"/pty-read-write", test_pty_read_write, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
