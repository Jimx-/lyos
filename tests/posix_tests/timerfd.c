#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/timerfd.h>
#include <string.h>
#include <poll.h>
#include <limits.h>
#include <lyos/limits.h>

#include "munit/munit.h"

#define NSEC_PER_SEC (1000000000ULL)

static MunitResult test_timerfd_read(const MunitParameter params[], void* data)
{
    struct timespec now;
    struct itimerspec curr_value, new_value;
    unsigned long sleep_nsecs = 100000000ULL; /* 100m */
    int fd;
    uint64_t exp;
    int retval, n;

    fd = timerfd_create(CLOCK_REALTIME, 0);
    munit_assert_int(fd, >=, 0);

    retval = clock_gettime(CLOCK_REALTIME, &now);
    munit_assert_int(retval, ==, 0);

    new_value.it_value.tv_sec = now.tv_sec;
    new_value.it_value.tv_nsec = now.tv_nsec;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    if (NSEC_PER_SEC - new_value.it_value.tv_nsec < sleep_nsecs) {
        new_value.it_value.tv_sec += 1;
        new_value.it_value.tv_nsec =
            sleep_nsecs - (NSEC_PER_SEC - new_value.it_value.tv_nsec);
    } else {
        new_value.it_value.tv_nsec += sleep_nsecs;
    }

    retval = timerfd_settime(fd, TFD_TIMER_ABSTIME, &new_value, NULL);
    munit_assert_int(retval, ==, 0);

    n = read(fd, &exp, sizeof(uint64_t));
    munit_assert_int(n, ==, sizeof(uint64_t));
    munit_assert_uint64(exp, ==, 1ULL);

    retval = clock_gettime(CLOCK_REALTIME, &now);
    munit_assert_int(retval, ==, 0);
    munit_assert(now.tv_sec > new_value.it_value.tv_sec ||
                 (now.tv_sec == new_value.it_value.tv_sec &&
                  now.tv_nsec >= new_value.it_value.tv_nsec));

    retval = timerfd_gettime(fd, &curr_value);
    munit_assert_int(retval, ==, 0);
    munit_assert(curr_value.it_value.tv_sec == 0 &&
                 curr_value.it_value.tv_nsec == 0);
    munit_assert(curr_value.it_interval.tv_sec == 0 &&
                 curr_value.it_interval.tv_nsec == 0);

    close(fd);

    return MUNIT_OK;
}

MunitTest timerfd_tests[] = {
    {(char*)"/read", test_timerfd_read, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
