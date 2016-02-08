#include "pthread.h"

#include "pthread_internal.h"

int pthread_exit(void* value_ptr)
{
	_exit(0);
}
