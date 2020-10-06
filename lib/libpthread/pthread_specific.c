#include <errno.h>
#include <pthread.h>

#include "pthread_internal.h"

typedef struct {
    void* value;
} pthread_key_internal_t;

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
    pthread_key_internal_t* pk = (pthread_key_internal_t*)key;
    pk->value = NULL;

    return 0;
}

void* pthread_getspecific(pthread_key_t key)
{
    pthread_key_internal_t* pk = (pthread_key_internal_t*)key;
    return pk->value;
}

int pthread_setspecific(pthread_key_t key, const void* value)
{
    pthread_key_internal_t* pk = (pthread_key_internal_t*)key;
    pk->value = value;

    return 0;
}

int pthread_key_delete(pthread_key_t key) { return 0; }
