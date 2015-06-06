#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

static void do_trace(pid_t child, int s);

int main(int argc, char* argv[])
{   
    if (argc == 1) return 0;

    pid_t child;
    child = fork();

    argv++;
    if(child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execve(argv[0], argv, NULL);
    }
    else {
        int status;
        wait(&status);
        do_trace(child, status);
    }
    return 0;
}

static void do_trace(pid_t child, int s)
{
    int status = s;

    while (1) {
        if (WIFEXITED(status)) break;

        ptrace(PTRACE_SYSCALL, child, 0, 0);

        wait(&status);
    }

    printf("+++ exited with %d +++\n", WEXITSTATUS(status));
}
