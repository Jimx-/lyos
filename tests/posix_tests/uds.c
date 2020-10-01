#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <string.h>
#include <poll.h>

#include "munit/munit.h"

static const char* server_path = "server.socket";
static const char* client_path = "client.socket";

static int run_client(int event_fd)
{
    int sock_fd;
    struct sockaddr_un cliun, serun;
    size_t addrlen, len;
    int retval, n;
    uint64_t count;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) return errno;

    memset(&cliun, 0, sizeof(cliun));
    cliun.sun_family = AF_UNIX;
    strcpy(cliun.sun_path, client_path);
    addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(cliun.sun_path);
    retval = bind(sock_fd, (struct sockaddr*)&cliun, addrlen);
    if (retval) return retval;

    n = read(event_fd, &count, sizeof(count));
    if (n < 0) return errno;

    memset(&serun, 0, sizeof(serun));
    serun.sun_family = AF_UNIX;
    strcpy(serun.sun_path, server_path);
    len = offsetof(struct sockaddr_un, sun_path) + strlen(serun.sun_path);
    retval = connect(sock_fd, (struct sockaddr*)&serun, len);
    if (retval < 0) return errno;

    return 0;
}

static MunitResult run_server(int event_fd)
{
    int listen_fd, conn_fd;
    struct sockaddr_un serun, cliun;
    socklen_t cliun_len;
    size_t addrlen;
    int retval, n;
    uint64_t count = 1;

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    munit_assert_int(listen_fd, >=, 0);

    memset(&serun, 0, sizeof(serun));
    serun.sun_family = AF_UNIX;
    strcpy(serun.sun_path, server_path);
    addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(serun.sun_path);
    retval = bind(listen_fd, (struct sockaddr*)&serun, addrlen);
    munit_assert_int(retval, ==, 0);

    retval = listen(listen_fd, 20);
    munit_assert_int(retval, ==, 0);

    n = write(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    cliun_len = sizeof(cliun);
    conn_fd = accept(listen_fd, (struct sockaddr*)&cliun, &cliun_len);
    munit_assert_int(conn_fd, >=, 0);
    munit_assert_string_equal(cliun.sun_path, client_path);

    return MUNIT_OK;
}

static MunitResult test_uds_read_write(const MunitParameter params[],
                                       void* data)
{
    pid_t cpid;
    MunitResult res;
    int fd;
    int status;

    fd = eventfd(0, 0);
    munit_assert_int(fd, >=, 0);

    cpid = fork();
    munit_assert_int(cpid, >=, 0);

    if (cpid == 0) {
        res = run_client(fd);

        if (res)
            _exit(EXIT_FAILURE);
        else
            _exit(EXIT_SUCCESS);
    } else {
        res = run_server(fd);
        wait(&status);
        munit_assert(WIFEXITED(status));
        munit_assert_int(WEXITSTATUS(status), ==, EXIT_SUCCESS);
    }

    close(fd);

    return res;
}

MunitTest uds_tests[] = {
    {(char*)"/read-write", test_uds_read_write, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
