#ifndef _PTHREAD_H_
#define _PTHREAD_H_

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/time.h>
#include <bits/pthreadtypes.h>

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2

#define PTHREAD_MUTEX_INITIALIZER          \
    {                                      \
        ((PTHREAD_MUTEX_NORMAL & 3) << 14) \
    }

#define PTHREAD_COND_INITIALIZER \
    {                            \
        0                        \
    }

#define PTHREAD_RWLOCK_INITIALIZER \
    {                              \
        0                          \
    }

#define PTHREAD_ONCE_INIT 0

#define PTHREAD_CREATE_DETACHED 0x00000001
#define PTHREAD_CREATE_JOINABLE 0x00000000

#define PTHREAD_INHERIT_SCHED  0
#define PTHREAD_EXPLICIT_SCHED 1

#define PTHREAD_SCOPE_SYSTEM  0
#define PTHREAD_SCOPE_PROCESS 1

__BEGIN_DECLS

int pthread_attr_init(pthread_attr_t* attr);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);
int pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                   void* (*start_routine)(void*), void* arg);
void pthread_exit(void* retval);
int pthread_join(pthread_t thread, void** retval);
int pthread_detach(pthread_t thread);
int pthread_cancel(pthread_t thread);

int pthread_getcpuclockid(pthread_t thread, clockid_t* clockid);

int pthread_condattr_init(pthread_condattr_t* attr);
int pthread_condattr_setclock(pthread_condattr_t* attr, clockid_t clockid);

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
                           const struct timespec* abs_time);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_destroy(pthread_cond_t* cond);

int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type_p);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_timedlock(pthread_mutex_t* mutex,
                            const struct timespec* abs_time);
int pthread_mutex_destroy(pthread_mutex_t* mutex);

int pthread_rwlock_init(pthread_rwlock_t* rwlock,
                        const pthread_rwlockattr_t* attr);
int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);

int pthread_once(pthread_once_t* once_control, void (*init_routine)(void));

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);
int pthread_key_delete(pthread_key_t key);

int pthread_barrier_init(pthread_barrier_t* restrict barrier,
                         const pthread_barrierattr_t* restrict attr,
                         unsigned count);
int pthread_barrier_wait(pthread_barrier_t* barrier);
int pthread_barrier_destroy(pthread_barrier_t* barrier);

__END_DECLS

#endif
