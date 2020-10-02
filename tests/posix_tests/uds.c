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
#include <sys/uio.h>

#include "munit/munit.h"

static const char* server_path = "server.socket";
static const char* client_path = "client.socket";
static const char test_string[] = "Hello, world!";

static const char test_string1[] = "Hello, ";
static const char test_string2[] = "world!";

static int run_client_simple(int event_fd)
{
    int sock_fd;
    struct sockaddr_un cliun, serun;
    size_t addrlen, len;
    char buf[100];
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

    /* client -> server */
    n = send(sock_fd, test_string, strlen(test_string), 0);
    if (n != strlen(test_string)) return errno;

    /* server -> client */
    n = recv(sock_fd, buf, strlen(test_string), 0);
    munit_assert_int(n, ==, strlen(test_string));
    buf[n] = '\0';
    munit_assert_string_equal(buf, test_string);

    close(sock_fd);

    unlink(client_path);

    return 0;
}

static MunitResult run_server_simple(int event_fd)
{
    int listen_fd, conn_fd;
    struct sockaddr_un serun, cliun;
    socklen_t cliun_len;
    size_t addrlen;
    int retval, n;
    char buf[100];
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

    /* client -> server */
    n = recv(conn_fd, buf, strlen(test_string), 0);
    munit_assert_int(n, ==, strlen(test_string));
    buf[n] = '\0';
    munit_assert_string_equal(buf, test_string);

    /* server -> client */
    n = send(conn_fd, test_string, strlen(test_string), 0);
    munit_assert_int(n, ==, strlen(test_string));

    close(conn_fd);
    close(listen_fd);

    unlink(server_path);

    return MUNIT_OK;
}

static MunitResult test_uds_send_recv_simple(const MunitParameter params[],
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
        res = run_client_simple(fd);

        if (res)
            _exit(EXIT_FAILURE);
        else
            _exit(EXIT_SUCCESS);
    } else {
        res = run_server_simple(fd);
        wait(&status);
        munit_assert(WIFEXITED(status));
        munit_assert_int(WEXITSTATUS(status), ==, EXIT_SUCCESS);
    }

    close(fd);

    return res;
}

static int run_client_msg(int event_fd)
{
    int sock_fd;
    struct sockaddr_un cliun, serun;
    size_t addrlen, len;
    struct iovec iov[2];
    struct msghdr msghdr;
    char buf[100];
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

    iov[0].iov_base = (void*)test_string1;
    iov[0].iov_len = strlen(test_string1);
    iov[1].iov_base = (void*)test_string2;
    iov[1].iov_len = strlen(test_string2);

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = iov;
    msghdr.msg_iovlen = 2;
    msghdr.msg_control = NULL;
    msghdr.msg_controllen = 0;

    n = sendmsg(sock_fd, &msghdr, 0);
    if (n != iov[0].iov_len + iov[1].iov_len) return errno;

    close(sock_fd);

    unlink(client_path);

    return 0;
}

static MunitResult run_server_msg(int event_fd)
{
    int listen_fd, conn_fd;
    struct sockaddr_un serun, cliun;
    socklen_t cliun_len;
    size_t addrlen;
    struct iovec iov;
    struct msghdr msghdr;
    int retval, n;
    char buf[100];
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

    iov.iov_base = buf;
    iov.iov_len = strlen(test_string);

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &iov;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = NULL;
    msghdr.msg_controllen = 0;

    n = recvmsg(conn_fd, &msghdr, 0);
    munit_assert_int(n, ==, iov.iov_len);
    buf[iov.iov_len] = '\0';
    munit_assert_string_equal(buf, test_string);

    close(conn_fd);
    close(listen_fd);

    unlink(server_path);

    return MUNIT_OK;
}

static MunitResult test_uds_send_recv_msg(const MunitParameter params[],
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
        res = run_client_msg(fd);

        if (res)
            _exit(EXIT_FAILURE);
        else
            _exit(EXIT_SUCCESS);
    } else {
        res = run_server_msg(fd);
        wait(&status);
        munit_assert(WIFEXITED(status));
        munit_assert_int(WEXITSTATUS(status), ==, EXIT_SUCCESS);
    }

    close(fd);

    return res;
}

MunitTest uds_tests[] = {
    {(char*)"/send-recv-simple", test_uds_send_recv_simple, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/send-recv-msg", test_uds_send_recv_msg, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
