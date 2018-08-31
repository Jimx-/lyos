#include "pthread.h"

#include "pthread_internal.h"

#include <string.h>
#include <errno.h>
#include <sys/futex.h>

typedef struct {
	unsigned int state;
	unsigned int owner;
} pthread_mutex_internal_t;

#define MUTEX_TYPE_MASK		(3 << 14)
#define MUTEX_TYPE_NORMAL 	(0 << 14)

#define MUTEX_STATE_MASK	3
#define MUTEX_STATE_UNLOCKED	0
#define MUTEX_STATE_LOCKED_UNCONTENDED	1
#define MUTEX_STATE_LOCKED_CONTENDED 	2

int pthread_mutex_init(pthread_mutex_t* pmutex, const pthread_mutexattr_t* attr)
{
	pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*) pmutex;
	memset(mutex, 0, sizeof(pthread_mutex_internal_t));

	if (!attr) {
		mutex->state = 0;
		return 0;
	}

	return 0;
}

static inline int __pthread_normal_mutex_trylock(pthread_mutex_internal_t* mutex)
{
	unsigned int unlocked = MUTEX_STATE_UNLOCKED;
	unsigned int locked_uncontended = MUTEX_STATE_LOCKED_UNCONTENDED;

	unsigned int old_state = unlocked;
	if (__sync_bool_compare_and_swap(&mutex->state, old_state, locked_uncontended)) {
		return 0;
	}
	return EBUSY;
}

static inline int __pthread_normal_mutex_lock(pthread_mutex_internal_t* mutex, const struct timespec* abs_time)
{
	if (__pthread_normal_mutex_trylock(mutex) == 0) return 0;

	unsigned int unlocked = MUTEX_STATE_UNLOCKED;
	unsigned int locked_contended = MUTEX_STATE_LOCKED_CONTENDED;

	while (__atomic_exchange_n(&mutex->state, locked_contended, __ATOMIC_ACQUIRE) != unlocked) {
		futex((int*)&mutex->state, FUTEX_WAIT, locked_contended, abs_time, NULL, 0);
	}

	return 0;
}

static inline void __pthread_normal_mutex_unlock(pthread_mutex_internal_t* mutex)
{
	unsigned int unlocked = MUTEX_STATE_UNLOCKED;
	unsigned int locked_contended = MUTEX_STATE_LOCKED_CONTENDED;

	if (__atomic_exchange_n(&mutex->state, unlocked, __ATOMIC_RELEASE) == locked_contended) {
		futex((int*)&mutex->state, FUTEX_WAKE, 1, NULL, NULL, 0);
	}
}

static int __pthread_mutex_lock_timeout(pthread_mutex_internal_t* mutex, const struct timespec* abs_time)
{
	unsigned int old_state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
	unsigned int type = old_state & MUTEX_TYPE_MASK;

	if (type == MUTEX_TYPE_NORMAL) {
		return __pthread_normal_mutex_lock(mutex, abs_time);
	}

	return 0;
}

int pthread_mutex_lock(pthread_mutex_t* pmutex) 
{
	pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*) pmutex;

	unsigned int old_state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
	unsigned int type = old_state & MUTEX_TYPE_MASK;

	if (type == MUTEX_TYPE_NORMAL) {
		if (__pthread_normal_mutex_trylock(mutex) == 0) return 0;
	}

	return __pthread_mutex_lock_timeout(mutex, NULL);
}

int pthread_mutex_unlock(pthread_mutex_t* pmutex) 
{
	pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*) pmutex;

	unsigned int old_state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
	unsigned int type = old_state & MUTEX_TYPE_MASK;

	if (type == MUTEX_TYPE_NORMAL) {
		__pthread_normal_mutex_unlock(mutex);
		return 0;
	}

	return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* pmutex)
{
	pthread_mutex_internal_t* mutex = (pthread_mutex_internal_t*) pmutex;

	unsigned int old_state = __atomic_load_n(&mutex->state, __ATOMIC_RELAXED);
	unsigned int type = old_state & MUTEX_TYPE_MASK;

	if (type == MUTEX_TYPE_NORMAL) {
		return __pthread_normal_mutex_trylock(mutex);
	}

	return EBUSY;
}
