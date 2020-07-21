#include <errno.h>
#include <pthread.h>

#include "pthread_internal.h"

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
    return ENOSYS;
}

void* pthread_getspecific(pthread_key_t key) { return NULL; }

int pthread_setspecific(pthread_key_t key, const void* value) { return ENOSYS; }

int pthread_key_delete(pthread_key_t key) { return ENOSYS; }
