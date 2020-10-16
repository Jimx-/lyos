#define _GNU_SOURCE
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

static const char test_string_pipe[] = "Hello from pipe!";

static int run_client_simple(int event_fd)
{
    int sock_fd;
    struct sockaddr_un cliun, serun;
    size_t addrlen, len;
    char buf[100];
    int retval, n;
    struct pollfd pfd;
    uint64_t count;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    munit_assert_int(sock_fd, >=, 0);

    memset(&cliun, 0, sizeof(cliun));
    cliun.sun_family = AF_UNIX;
    strcpy(cliun.sun_path, client_path);
    addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(cliun.sun_path);
    retval = bind(sock_fd, (struct sockaddr*)&cliun, addrlen);
    munit_assert_int(retval, ==, 0);

    n = read(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    memset(&serun, 0, sizeof(serun));
    serun.sun_family = AF_UNIX;
    strcpy(serun.sun_path, server_path);
    len = offsetof(struct sockaddr_un, sun_path) + strlen(serun.sun_path);
    retval = connect(sock_fd, (struct sockaddr*)&serun, len);
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
    struct pollfd pfd;
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
    munit_assert_int(cliun_len, ==,
                     offsetof(struct sockaddr_un, sun_path) +
                         strlen(client_path) + 1);
    munit_assert_string_equal(cliun.sun_path, client_path);

    pfd.fd = conn_fd;
    pfd.events = POLLIN;
    n = poll(&pfd, 1, 0);
    munit_assert_int(n, ==, 0);

    n = write(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    /* client -> server */
    cliun_len = sizeof(cliun);
    memset(&cliun, 0, cliun_len);
    n = recvfrom(conn_fd, buf, strlen(test_string), 0, (struct sockaddr*)&cliun,
                 &cliun_len);
    munit_assert_int(n, ==, strlen(test_string));
    buf[n] = '\0';
    munit_assert_string_equal(buf, test_string);
    munit_assert_int(cliun_len, ==,
                     offsetof(struct sockaddr_un, sun_path) +
                         strlen(client_path) + 1);
    munit_assert_string_equal(cliun.sun_path, client_path);

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
    msghdr.msg_flags = 0;

    n = sendmsg(sock_fd, &msghdr, 0);
    if (n != iov[0].iov_len + iov[1].iov_len) return errno;

    n = read(event_fd, &count, sizeof(count));
    if (n < 0) return errno;

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
    munit_assert_int(cliun_len, ==,
                     offsetof(struct sockaddr_un, sun_path) +
                         strlen(client_path) + 1);
    munit_assert_string_equal(cliun.sun_path, client_path);

    iov.iov_base = buf;
    iov.iov_len = strlen(test_string);

    memset(&cliun, 0, sizeof(cliun));
    msghdr.msg_name = &cliun;
    msghdr.msg_namelen = sizeof(cliun);
    msghdr.msg_iov = &iov;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = NULL;
    msghdr.msg_controllen = 0;
    msghdr.msg_flags = 0;

    n = recvmsg(conn_fd, &msghdr, 0);
    munit_assert_int(n, ==, iov.iov_len);
    buf[iov.iov_len] = '\0';
    munit_assert_string_equal(buf, test_string);

    munit_assert_int(msghdr.msg_namelen, ==,
                     offsetof(struct sockaddr_un, sun_path) +
                         strlen(client_path) + 1);
    munit_assert_string_equal(cliun.sun_path, client_path);

    n = write(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

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

static int run_client_cred(int event_fd, pid_t server_pid)
{
    int sock_fd;
    struct sockaddr_un cliun, serun;
    size_t addrlen, len;
    int retval, n;
    struct ucred ucred;
    socklen_t cred_len;
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

    cred_len = sizeof(ucred);
    retval = getsockopt(sock_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &cred_len);
    munit_assert_int(retval, ==, 0);
    munit_assert_int(cred_len, ==, sizeof(ucred));
    munit_assert_int(ucred.pid, ==, server_pid);

    n = read(event_fd, &count, sizeof(count));
    if (n < 0) return errno;

    close(sock_fd);

    unlink(client_path);

    return 0;
}

static MunitResult run_server_cred(int event_fd, pid_t client_pid)
{
    int listen_fd, conn_fd;
    struct sockaddr_un serun, cliun;
    socklen_t cliun_len;
    size_t addrlen;
    int retval, n;
    struct ucred ucred;
    socklen_t cred_len;
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
    munit_assert_int(cliun_len, ==,
                     offsetof(struct sockaddr_un, sun_path) +
                         strlen(client_path) + 1);
    munit_assert_string_equal(cliun.sun_path, client_path);

    cred_len = sizeof(ucred);
    retval = getsockopt(conn_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &cred_len);
    munit_assert_int(retval, ==, 0);
    munit_assert_int(cred_len, ==, sizeof(ucred));
    munit_assert_int(ucred.pid, ==, client_pid);

    n = write(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    close(conn_fd);
    close(listen_fd);

    unlink(server_path);

    return MUNIT_OK;
}

static MunitResult test_uds_send_recv_cred(const MunitParameter params[],
                                           void* data)
{
    pid_t pid, cpid;
    MunitResult res;
    int fd;
    int status;

    pid = getpid();
    munit_assert_int(pid, >, 0);

    fd = eventfd(0, 0);
    munit_assert_int(fd, >=, 0);

    cpid = fork();
    munit_assert_int(cpid, >=, 0);

    if (cpid == 0) {
        res = run_client_cred(fd, pid);

        if (res)
            _exit(EXIT_FAILURE);
        else
            _exit(EXIT_SUCCESS);
    } else {
        res = run_server_cred(fd, cpid);
        wait(&status);
        munit_assert(WIFEXITED(status));
        munit_assert_int(WEXITSTATUS(status), ==, EXIT_SUCCESS);
    }

    close(fd);

    return res;
}

static MunitResult test_socketpair(const MunitParameter params[], void* data)
{
    char rbuf[100];
    int fds[2];
    int retval;
    size_t n;

    retval = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    munit_assert_int(retval, ==, 0);

    n = write(fds[0], test_string, strlen(test_string));
    munit_assert_int(n, ==, strlen(test_string));

    n = read(fds[1], rbuf, sizeof(rbuf));
    munit_assert_int(n, ==, strlen(test_string));
    rbuf[n] = '\0';
    munit_assert_string_equal(rbuf, test_string);

    close(fds[0]);
    close(fds[1]);

    return MUNIT_OK;
}

static int run_client_fds(int event_fd)
{
    int sock_fd;
    struct sockaddr_un cliun, serun;
    size_t addrlen, len;
    struct iovec iov;
    struct msghdr msghdr;
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;
    struct cmsghdr* cmsg;
    int retval, n;
    int fds[2];
    uint64_t count;

    retval = pipe(fds);
    if (retval < 0) return errno;

    n = write(fds[1], test_string_pipe, strlen(test_string_pipe));
    munit_assert_int(n, ==, strlen(test_string_pipe));

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

    iov.iov_base = (void*)test_string;
    iov.iov_len = strlen(test_string);

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &iov;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = u.buf;
    msghdr.msg_controllen = sizeof(u.buf);
    msghdr.msg_flags = 0;

    /* send the read end of the pipe */
    cmsg = CMSG_FIRSTHDR(&msghdr);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fds[0]));
    memcpy(CMSG_DATA(cmsg), &fds[0], sizeof(fds[0]));

    n = sendmsg(sock_fd, &msghdr, 0);
    if (n != iov.iov_len) return errno;

    n = read(event_fd, &count, sizeof(count));
    if (n < 0) return errno;

    close(sock_fd);
    close(fds[0]);
    close(fds[1]);

    unlink(client_path);

    return 0;
}

static MunitResult run_server_fds(int event_fd)
{
    int listen_fd, conn_fd;
    struct sockaddr_un serun, cliun;
    socklen_t cliun_len;
    size_t addrlen;
    struct iovec iov;
    struct msghdr msghdr;
    int pipe_fd;

    union {
        char buf[CMSG_SPACE(sizeof(pipe_fd))];
        struct cmsghdr align;
    } u;
    struct cmsghdr* cmsg;

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
    munit_assert_int(cliun_len, ==,
                     offsetof(struct sockaddr_un, sun_path) +
                         strlen(client_path) + 1);
    munit_assert_string_equal(cliun.sun_path, client_path);

    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &iov;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = u.buf;
    msghdr.msg_controllen = sizeof(u.buf);
    msghdr.msg_flags = 0;

    n = recvmsg(conn_fd, &msghdr, 0);

    munit_assert_int(n, ==, strlen(test_string));
    buf[n] = '\0';
    munit_assert_string_equal(buf, test_string);
    munit_assert_int(msghdr.msg_controllen, ==, sizeof(u.buf));

    cmsg = CMSG_FIRSTHDR(&msghdr);
    munit_assert_int(cmsg->cmsg_level, ==, SOL_SOCKET);
    munit_assert_int(cmsg->cmsg_type, ==, SCM_RIGHTS);
    munit_assert_int(cmsg->cmsg_len, ==, CMSG_LEN(sizeof(pipe_fd)));

    memcpy(&pipe_fd, CMSG_DATA(cmsg), sizeof(pipe_fd));
    munit_assert_int(pipe_fd, >=, 0);

    n = read(pipe_fd, buf, sizeof(buf));
    munit_assert_int(n, ==, strlen(test_string_pipe));
    buf[n] = '\0';
    munit_assert_string_equal(buf, test_string_pipe);

    n = write(event_fd, &count, sizeof(count));
    munit_assert_int(n, ==, sizeof(count));

    close(conn_fd);
    close(listen_fd);
    close(pipe_fd);

    unlink(server_path);

    return MUNIT_OK;
}

static MunitResult test_uds_send_recv_fds(const MunitParameter params[],
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
        res = run_client_fds(fd);

        if (res)
            _exit(EXIT_FAILURE);
        else
            _exit(EXIT_SUCCESS);
    } else {
        res = run_server_fds(fd);
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
    {(char*)"/send-recv-cred", test_uds_send_recv_cred, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/send-recv-fds", test_uds_send_recv_fds, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/socketpair", test_socketpair, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};
