#ifndef _UCONTEXT_H_
#define _UCONTEXT_H_

#include <sys/ucontext.h>

int getcontext(ucontext_t* ucp);
int setcontext(const ucontext_t* ucp);
void makecontext(ucontext_t* ucp, void (*func)(void), int argc, ...);
int swapcontext(ucontext_t* oucp, const ucontext_t* ucp);

#endif
