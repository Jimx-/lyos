#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

void pthread_cleanup_push(void (*routine)(void*), void* arg) {}

void pthread_cleanup_pop(int execute) {}
