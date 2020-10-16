#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>

#include "munit/munit.h"

static MunitResult test_shared_anon(const MunitParameter params[], void* data)
{
    int* buf;
    pid_t cpid;
    int fds[2];
    int retval;
    int n, count;

    buf = mmap(NULL, sizeof(*buf), PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANON, -1, 0);
    munit_assert_ptr(buf, !=, MAP_FAILED);

    retval = pipe(fds);
    munit_assert_int(retval, ==, 0);

    cpid = fork();
    munit_assert_int(cpid, >=, 0);

    if (cpid == 0) {
        close(fds[0]);

        *buf = 100;
        write(fds[1], &count, sizeof(count));

        close(fds[1]);
        _exit(0);
    } else {
        close(fds[1]);

        n = read(fds[0], &count, sizeof(count));
        munit_assert_int(n, ==, sizeof(count));
        close(fds[0]);

        munit_assert_int(*buf, ==, 100);

        wait(NULL);
        munmap(buf, sizeof(*buf));
    }

    return MUNIT_OK;
}

MunitTest mmap_tests[] = {
    {(char*)"/shared-anon", test_shared_anon, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
