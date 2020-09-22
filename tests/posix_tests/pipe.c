#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "munit/munit.h"

static MunitResult test_pipe_read_write(const MunitParameter params[],
                                        void* data)
{
    int fds[2];
    pid_t cpid;
    char wstr[] = "hello";
    char rstr[10], *prstr = rstr;
    char buf;

    pipe(fds);

    cpid = fork();
    munit_assert_int(cpid, >=, 0);

    if (cpid == 0) {
        close(fds[0]);
        write(fds[1], wstr, 5);
        close(fds[1]);

        _exit(0);
    } else {
        close(fds[1]);

        while (read(fds[0], &buf, 1) > 0)
            *prstr++ = buf;
        *prstr = 0;

        munit_assert_string_equal(rstr, wstr);

        close(fds[0]);
        wait(NULL);
    }

    return MUNIT_OK;
}

MunitTest pipe_tests[] = {
    {(char*)"/read-write", test_pipe_read_write, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
