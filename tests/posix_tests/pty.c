#define _GNU_SOURCE
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>

#include "munit/munit.h"

static MunitResult test_pty_initial_winsize(const MunitParameter params[],
                                            void* data)
{
    struct winsize size = {24, 80, 0, 0}, actual;
    int master_fd, status;
    pid_t pid;

    /* Leave a reaped process slot, as a shell does after running a command. */
    pid = fork();
    munit_assert_int(pid, >=, 0);
    if (pid == 0) _exit(0);
    munit_assert_int(waitpid(pid, &status, 0), ==, pid);
    munit_assert(WIFEXITED(status));
    munit_assert_int(WEXITSTATUS(status), ==, 0);

    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    munit_assert_int(master_fd, >=, 0);

    /* Set the size before opening the slave or assigning a process group. */
    munit_assert_int(ioctl(master_fd, TIOCSWINSZ, &size), ==, 0);
    munit_assert_int(ioctl(master_fd, TIOCGWINSZ, &actual), ==, 0);
    munit_assert_int(actual.ws_row, ==, size.ws_row);
    munit_assert_int(actual.ws_col, ==, size.ws_col);
    munit_assert_int(close(master_fd), ==, 0);

    return MUNIT_OK;
}

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
    {(char*)"/pty-initial-winsize", test_pty_initial_winsize, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/pty-read-write", test_pty_read_write, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
