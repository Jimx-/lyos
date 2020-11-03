#include <errno.h>
#include <pthread.h>

#include "pthread_internal.h"

typedef struct {
    int used;
    void* value;
} pthread_key_internal_t;

#define NR_KEYS 100

static pthread_key_internal_t keys[NR_KEYS];

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*))
{
    int i;
    for (i = 0; i < NR_KEYS; i++)
        if (!keys[i].used) {
            keys[i].used = 1;
            keys[i].value = NULL;
            break;
        }

    if (i == NR_KEYS) return ENOMEM;

    *key = i;
    return 0;
}

void* pthread_getspecific(pthread_key_t key) { return keys[(int)key].value; }

int pthread_setspecific(pthread_key_t key, const void* value)
{
    keys[(int)key].value = value;

    return 0;
}

int pthread_key_delete(pthread_key_t key) { return 0; }
