#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <sys/cdefs.h>
#include <time.h>

#define SEM_VALUE_MAX 0x7FFFFFFF

typedef struct {
    unsigned int count;
} sem_t;

__BEGIN_DECLS

int sem_init(sem_t* sem, int pshared, unsigned int initial_count);
sem_t* sem_open(const char*, int, ...);
int sem_close(sem_t* sem);
int sem_unlink(const char*);
int sem_destroy(sem_t* sem);
int sem_wait(sem_t* sem);
int sem_trywait(sem_t* sem);
int sem_timedwait(sem_t* sem, const struct timespec* abstime);
int sem_post(sem_t* sem);
int sem_getvalue(sem_t* sem, int* sval);

__END_DECLS

#endif /* _SEMAPHORE_H */
