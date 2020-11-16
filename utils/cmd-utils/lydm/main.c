#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

static const char* prog_name = "lydm";

static int init_udev(void)
{
    pid_t udev_pid, pid;

    printf("%s: starting udevd\n", prog_name);

    udev_pid = fork();
    assert(udev_pid != -1);

    if (!udev_pid) {
        // char* rc_args[] = {"/usr/bin/strace", "/usr/sbin/udevd", "--debug",
        //                   NULL};
        char* rc_args[] = {"/usr/sbin/udevd", NULL};
        char* rc_env[] = {NULL};
        // execve("/usr/bin/strace", rc_args, rc_env);
        execve("/usr/sbin/udevd", rc_args, rc_env);
        exit(EXIT_FAILURE);
    }

    while (access("/run/udev/rules.d", F_OK)) {
        assert(errno == ENOENT);
        usleep(100);
    }

    printf("%s: running udevadm-trigger\n", prog_name);

    pid = fork();
    assert(pid != -1);

    if (pid)
        waitpid(pid, NULL, 0);
    else {
        char* rc_args[] = {"/usr/bin/udevadm", "trigger", "--action=add", NULL};
        char* rc_env[] = {NULL};
        execve("/usr/bin/udevadm", rc_args, rc_env);
        exit(EXIT_FAILURE);
    }

    printf("%s: running udevadm-settle\n", prog_name);

    pid = fork();
    assert(pid != -1);

    if (pid)
        waitpid(pid, NULL, 0);
    else {
        char* rc_args[] = {"/usr/bin/udevadm", "settle", NULL};
        char* rc_env[] = {NULL};
        execve("/usr/bin/udevadm", rc_args, rc_env);
        exit(EXIT_FAILURE);
    }

    printf("%s: udev initialization done\n", prog_name);

    return 0;
}

int main()
{
    init_udev();

    return 0;
}
