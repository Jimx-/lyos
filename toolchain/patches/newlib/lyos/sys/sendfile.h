#ifndef _SYS_SENDFILE_H_
#define _SYS_SENDFILE_H_

#include <sys/types.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

ssize_t sendfile(int, int, off_t*, size_t);

__END_DECLS

#endif
