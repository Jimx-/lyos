#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

#include "munit/munit.h"

static MunitResult test_dlsym(const MunitParameter params[], void* data)
{
    size_t (*_strlen)(const char*) = dlsym(RTLD_NEXT, "strlen");
    munit_assert_not_null(_strlen);

    munit_assert_int(_strlen("test"), ==, strlen("test"));
    return MUNIT_OK;
}

MunitTest dl_tests[] = {
    {(char*)"/dlsym", test_dlsym, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
