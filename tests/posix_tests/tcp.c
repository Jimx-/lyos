#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <string.h>
#include <poll.h>

#include "munit/munit.h"

static const char test_string[] = "Hello, world!";

static MunitResult run_server_simple(int event_fd)
{
    int listen_fd, conn_fd;
    struct sockaddr_in serin, cliin;
    socklen_t cliin_len;
    char buf[100];
    uint64_t count = 1;
    struct pollfd pfd;
    int retval, n;

    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    munit_assert_int(listen_fd, >=, 0);

    memset(&serin, 0, sizeof(serin));
    serin.sin_family = AF_INET;
    serin.sin_port = htons(8080);
    serin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    retval = bind(listen_fd, (struct sockaddr*)&serin, sizeof(serin));
    munit_assert_int(retval, ==, 0);

    retval = listen(listen_fd, 20);
    munit_assert_int(retval, ==, 0);

    n = write(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    conn_fd = accept(listen_fd, (struct sockaddr*)&cliin, &cliin_len);
    munit_assert_int(conn_fd, >=, 0);

    pfd.fd = conn_fd;
    pfd.events = POLLIN;
    n = poll(&pfd, 1, 0);
    munit_assert_int(n, ==, 0);

    n = write(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    n = recv(conn_fd, buf, strlen(test_string), 0);
    munit_assert_int(n, ==, strlen(test_string));

    /* server -> client */
    n = send(conn_fd, test_string, strlen(test_string), 0);
    munit_assert_int(n, ==, strlen(test_string));

    n = write(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    /* wait for the client to close the connection */
    pfd.fd = conn_fd;
    pfd.events = POLLIN;
    n = poll(&pfd, 1, -1);
    munit_assert_int(n, ==, 1);

    close(conn_fd);
    close(listen_fd);

    return MUNIT_OK;
}

static MunitResult run_client_simple(int event_fd)
{
    int sock_fd;
    struct sockaddr_in serin;
    uint64_t count;
    char buf[100];
    struct pollfd pfd;
    int retval, n;

    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    munit_assert_int(sock_fd, >=, 0);

    memset(&serin, 0, sizeof(serin));
    serin.sin_family = AF_INET;
    serin.sin_port = htons(8080);
    serin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    n = read(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    retval = connect(sock_fd, (struct sockaddr*)&serin, sizeof(serin));
    munit_assert_int(retval, ==, 0);

    n = read(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    /* client -> server */
    n = send(sock_fd, test_string, strlen(test_string), 0);
    munit_assert_int(n, ==, strlen(test_string));

    n = read(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    pfd.fd = sock_fd;
    pfd.events = POLLIN;
    n = poll(&pfd, 1, 0);
    munit_assert_int(n, ==, 1);
    munit_assert(pfd.revents & POLLIN);

    /* server -> client */
    n = recv(sock_fd, buf, strlen(test_string), 0);
    munit_assert_int(n, ==, strlen(test_string));

    close(sock_fd);

    return MUNIT_OK;
}

static MunitResult test_tcp_send_recv_simple(const MunitParameter params[],
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

MunitTest tcp_tests[] = {
    {(char*)"/tcp-send-recv-simple", test_tcp_send_recv_simple, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
