#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>

#define GETTY  "/usr/bin/getty"
#define NR_TTY 4

int main(int argc, char* argv[])
{
    int fd_stdin, fd_stdout, fd_stderr;
    pid_t pid;

    pid = fork();
    if (pid) {
        waitpid(pid, NULL, 0);
    } else {
        char* rc_args[] = {"/bin/sh", "/etc/rc", NULL};
        char* rc_env[] = {NULL};
        execve("/bin/sh", rc_args, rc_env);
    }

    fd_stdin = open("/dev/console", O_RDWR);
    fd_stdout = dup(fd_stdin);
    fd_stderr = dup(fd_stdin);

    pid = fork();
    if (!pid) {
        int fd;

        fd = open("/dev/tty6", O_RDWR);
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);

        char* rc_args[] = {"/usr/bin/lydm", NULL};
        char* rc_env[] = {NULL};
        execve("/usr/bin/lydm", rc_args, rc_env);
        exit(EXIT_FAILURE);
    }

    /* set hostname */
    int fd_hostname = open("/etc/hostname", O_RDONLY);
    if (fd_hostname != -1) {
        char hostname[256];
        memset(hostname, 0, sizeof(hostname));
        int len = read(fd_hostname, hostname, sizeof(hostname));
        printf("Setting hostname to %s\n", hostname);
        sethostname(hostname, len);
        close(fd_hostname);
    }

    char* ttylist[NR_TTY] = {"/dev/tty1", "/dev/tty2", "/dev/tty3",
                             "/dev/ttyS0"};
    int i;
    for (i = 0; i < NR_TTY; i++) {
        int pid = fork();
        if (pid) {
            /* printf("Parent\n"); */
        } else {
            close(0);
            close(1);
            close(2);

            assert(open(ttylist[i], O_RDWR) == 0);
            assert(open(ttylist[i], O_RDWR) == 1);
            assert(open(ttylist[i], O_RDWR) == 2);

            char* argv[] = {GETTY, NULL};
            _exit(execv(GETTY, argv));
        }
    }

    while (1) {
        int s;
        wait(&s);
    }

    return 0;
}
