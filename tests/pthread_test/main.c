#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 5

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

void* print_hello(void* thread_id)
{
    pthread_t tid;
    tid = pthread_self();

    pthread_mutex_lock(&print_mutex);
    printf("Hello World! It's me, thread #%ld!\n", tid);
    pthread_mutex_unlock(&print_mutex);

    return NULL;
}

int main()
{
    pthread_t threads[NUM_THREADS];
    int rc;
    long t;
    pthread_t main_tid;

    main_tid = pthread_self();

    for (t = 0; t < NUM_THREADS; t++) {
        pthread_mutex_lock(&print_mutex);
        printf("In main #%ld: creating thread %ld\n", main_tid, t);
        pthread_mutex_unlock(&print_mutex);

        rc = pthread_create(&threads[t], NULL, print_hello, (void*)t);
        if (rc) {
            printf("ERROR: return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    for (t = 0; t < NUM_THREADS; t++) {
        rc = pthread_join(threads[t], NULL);
        if (rc) {
            printf("ERROR: return code from pthread_join() is %d\n", rc);
            exit(-1);
        }
    }

    pthread_exit(NULL);
    return 0;
}
