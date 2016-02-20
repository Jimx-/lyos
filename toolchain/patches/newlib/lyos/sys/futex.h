#ifndef _SYS_FUTEX_H_
#define _SYS_FUTEX_H_

#include <sys/time.h>

#define FUTEX_WAIT	0
#define FUTEX_WAKE	1

#define FUTEX_BITSET_MATCH_ANY  0xffffffff

int futex(int *uaddr, int futex_op, int val,
	const struct timespec *timeout,   /* or: uint32_t val2 */
    int *uaddr2, int val3);

#endif
