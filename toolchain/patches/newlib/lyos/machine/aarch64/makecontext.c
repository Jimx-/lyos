#include <ucontext.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

void ctx_wrapper();

void makecontext(ucontext_t* ucp, void (*func)(void), int argc, ...) {}
