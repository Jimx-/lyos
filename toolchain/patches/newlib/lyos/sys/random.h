#ifndef _SYS_RANDOM_H
#define _SYS_RANDOM_H

#include <sys/types.h>
#include <sys/cdefs.h>

#define GRND_RANDOM   1
#define GRND_NONBLOCK 2

__BEGIN_DECLS

ssize_t getrandom(void*, size_t, unsigned int);

__END_DECLS

#endif /* _SYS_RANDOM_H */
