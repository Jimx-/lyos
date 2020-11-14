#ifndef _PTY_H
#define _PTY_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <termios.h>

__BEGIN_DECLS

int openpty(int* amaster, int* aslave, char* name, const struct termios* termp,
            const struct winsize* winp);

pid_t forkpty(int* amaster, char* name, const struct termios* termp,
              const struct winsize* winp);

__END_DECLS

#endif
