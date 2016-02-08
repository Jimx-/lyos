#ifndef _BITS_PTHREAD_TYPES_H_
#define _BITS_PTHREAD_TYPES_H_

#include <sys/types.h>
#include <sys/sched.h>

typedef long pthread_t;

typedef struct {
	int detachstate;
	int schedpolicy;
	struct sched_param schedparam;
	int inheritsched;
	int scope;
	size_t guardsize;
	int stackaddr_set;
	void* stackaddr;
	size_t stacksize;
} pthread_attr_t;

#endif
