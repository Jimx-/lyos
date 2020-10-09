#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/signal.h>

__BEGIN_DECLS

#define WCONTINUED 1
#define WNOHANG    2
#define WUNTRACED  4
#define WEXITED    8
#define WNOWAIT    16
#define WSTOPPED   32

#define WCOREFLAG 0x80

/* A status looks like:
      <2 bytes info> <2 bytes code>

      <code> == 0, child has exited, info is the exit value
      <code> == 1..7e, child has exited, info is the signal number.
      <code> == 7f, child has stopped, info was the signal number.
      <code> == 80, there was a core dump.
*/

#define WIFEXITED(w)   (((w)&0xff) == 0)
#define WIFSIGNALED(w) (((w)&0x7f) > 0 && (((w)&0x7f) < 0x7f))
#define WIFSTOPPED(w)  (((w)&0xff) == 0x7f)
#define WEXITSTATUS(w) (((w) >> 8) & 0xff)
#define WTERMSIG(w)    ((w)&0x7f)
#define WSTOPSIG       WEXITSTATUS

#define W_STOPCODE(sig)      ((sig) << 8 | 0x7f)
#define W_EXITCODE(ret, sig) ((ret) << 8 | (sig))

typedef enum { P_ALL, P_PID, P_PGID } idtype_t;

pid_t wait(int*);
pid_t waitpid(pid_t, int*, int);

#ifdef _COMPILING_NEWLIB
pid_t _wait(int*);
#endif

/* Provide prototypes for most of the _<systemcall> names that are
   provided in newlib for some compilers.  */
pid_t _wait(int*);

int waitid(idtype_t idtype, id_t id, siginfo_t* siginfo, int flags);

__END_DECLS

#endif
