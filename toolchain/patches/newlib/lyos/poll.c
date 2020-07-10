#include <poll.h>
#include <errno.h>

int poll(struct pollfd* fds, nfds_t nfds, int timeout) { return ENOSYS; }
