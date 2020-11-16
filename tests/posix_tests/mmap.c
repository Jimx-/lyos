#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
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

static MunitResult test_shared_filemap(const MunitParameter params[],
                                       void* data)
{
#define TEST_BUF_SIZE 8192
    char template[] = "/tmp/mmap-test-XXXXXX";
    unsigned char* buf;
    pid_t cpid;
    int tmpfd;
    int fds[2];
    int retval;
    int i, n, count, ok;

    tmpfd = mkstemp(template);
    munit_assert_int(tmpfd, >=, 0);
    unlink(template);

    retval = ftruncate(tmpfd, TEST_BUF_SIZE);
    munit_assert_int(retval, ==, 0);

    retval = pipe(fds);
    munit_assert_int(retval, ==, 0);

    cpid = fork();
    munit_assert_int(cpid, >=, 0);

    if (cpid == 0) {
        close(fds[0]);

        buf = mmap(NULL, TEST_BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                   tmpfd, 0);
        munit_assert_ptr(buf, !=, MAP_FAILED);

        close(tmpfd);

        for (i = 0; i < TEST_BUF_SIZE; i++) {
            buf[i] = i & 0xff;
        }

        write(fds[1], &count, sizeof(count));

        close(fds[1]);
        _exit(0);
    } else {
        close(fds[1]);

        buf = mmap(NULL, TEST_BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                   tmpfd, 0);
        munit_assert_ptr(buf, !=, MAP_FAILED);

        n = read(fds[0], &count, sizeof(count));
        munit_assert_int(n, ==, sizeof(count));
        close(fds[0]);

        ok = 1;
        for (i = 0; i < TEST_BUF_SIZE; i++) {
            if (buf[i] != (unsigned char)(i & 0xff)) {
                ok = 0;
                break;
            }
        }
        munit_assert(ok);

        wait(NULL);
        munmap(buf, TEST_BUF_SIZE);
    }

    close(tmpfd);

    return MUNIT_OK;
}

MunitTest mmap_tests[] = {
    {(char*)"/shared-anon", test_shared_anon, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/shared-filemap", test_shared_filemap, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
