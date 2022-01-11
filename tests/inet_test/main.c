#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <poll.h>

static const char test_string[] = "Hello, world!";

int run_server(int event_fd)
{
    int listen_fd, conn_fd;
    struct sockaddr_in serin, cliin;
    socklen_t cliin_len;
    char buf[100];
    uint64_t count = 1;
    struct pollfd pfd;
    int retval, n;

    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);

    memset(&serin, 0, sizeof(serin));
    serin.sin_family = AF_INET;
    serin.sin_port = htons(8080);
    serin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    retval = bind(listen_fd, (struct sockaddr*)&serin, sizeof(serin));
    assert(retval == 0);

    retval = listen(listen_fd, 20);
    assert(retval == 0);

    n = write(event_fd, &count, sizeof(count));
    assert(n == sizeof(count));

    conn_fd = accept(listen_fd, (struct sockaddr*)&cliin, &cliin_len);
    assert(conn_fd >= 0);

    pfd.fd = conn_fd;
    pfd.events = POLLIN;
    n = poll(&pfd, 1, 0);
    assert(n == 0);

    n = write(event_fd, &count, sizeof(count));
    assert(n == sizeof(count));

    n = recv(conn_fd, buf, strlen(test_string), 0);
    assert(n == strlen(test_string));

    /* server -> client */
    n = send(conn_fd, test_string, strlen(test_string), 0);
    assert(n == strlen(test_string));

    n = write(event_fd, &count, sizeof(count));
    assert(n == sizeof(count));

    /* wait for the client to close the connection */
    pfd.fd = conn_fd;
    pfd.events = POLLIN;
    n = poll(&pfd, 1, -1);
    assert(n == 1);

    close(conn_fd);
    close(listen_fd);

    return 0;
}

int run_client(int event_fd)
{
    int sock_fd;
    struct sockaddr_in serin;
    uint64_t count;
    char buf[100];
    struct pollfd pfd;
    int retval, n;

    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock_fd >= 0);

    memset(&serin, 0, sizeof(serin));
    serin.sin_family = AF_INET;
    serin.sin_port = htons(8080);
    serin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    n = read(event_fd, &count, sizeof(count));
    assert(n == sizeof(count));

    retval = connect(sock_fd, (struct sockaddr*)&serin, sizeof(serin));
    assert(retval == 0);

    n = read(event_fd, &count, sizeof(count));
    assert(n == sizeof(count));

    /* client -> server */
    n = send(sock_fd, test_string, strlen(test_string), 0);
    assert(n == strlen(test_string));

    n = read(event_fd, &count, sizeof(count));
    assert(n == sizeof(count));

    pfd.fd = sock_fd;
    pfd.events = POLLIN;
    n = poll(&pfd, 1, 0);
    assert(n == 1);
    assert(pfd.revents & POLLIN);

    /* server -> client */
    n = recv(sock_fd, buf, strlen(test_string), 0);
    assert(n == strlen(test_string));

    close(sock_fd);

    return 0;
}

int main()
{
    pid_t cpid;
    int fd;
    int status;
    int res;

    fd = eventfd(0, 0);

    cpid = fork();

    if (cpid == 0) {
        res = run_client(fd);

        if (res)
            _exit(EXIT_FAILURE);
        else
            _exit(EXIT_SUCCESS);
    } else {
        run_server(fd);
        wait(&status);
    }

    close(fd);

    return 0;
}
