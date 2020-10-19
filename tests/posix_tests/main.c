#include "munit/munit.h"

#include "suites.h"

static MunitTest test_suite_tests[] = {
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite all_suites[] = {
    {(char*)"/pipe", pipe_tests, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    {(char*)"/eventfd", eventfd_tests, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    {(char*)"/signalfd", signalfd_tests, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    {(char*)"/timerfd", timerfd_tests, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    {(char*)"/epoll", epoll_tests, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    {(char*)"/uds", uds_tests, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    {(char*)"/dl", dl_tests, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    {(char*)"/mmap", mmap_tests, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    {(char*)"/netlink", netlink_tests, NULL, 0, MUNIT_SUITE_OPTION_NONE},
    {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
};

static const MunitSuite test_suite = {(char*)"", test_suite_tests,
                                      (MunitSuite*)all_suites, 0,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)])
{
    return munit_suite_main(&test_suite, (void*)"posix", argc, argv);
}
