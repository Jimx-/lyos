#include <sys/wait.h>
#include <errno.h>

int waitid(idtype_t idtype, id_t id, siginfo_t* siginfo, int flags)
{
    int status, retval = 0;
    pid_t pid = 0;

    if (idtype == P_PID)
        pid = waitpid(id, &status, 0);
    else if (idtype == P_PGID)
        pid = waitpid(-id, &status, 0);
    else if (idtype == P_ALL)
        pid = wait(&status);
    else
        retval = EINVAL;

    if (pid < 0) retval = -pid;

    if (retval) goto err;

    siginfo->si_signo = SIGCHLD;
    siginfo->si_pid = pid;
    siginfo->si_errno = 0;
    siginfo->si_status = WEXITSTATUS(status);

    if (WIFEXITED(status))
        siginfo->si_code = CLD_EXITED;
    else if (WIFSIGNALED(status))
        siginfo->si_code = CLD_KILLED;
    else if (WIFSTOPPED(status))
        siginfo->si_code = CLD_STOPPED;

    return 0;

err:
    errno = retval;
    return -1;
}
