#ifndef _LIBSOCKDRIVER_RPOTO_H_
#define _LIBSOCKDRIVER_PROTO_H_

#include "worker.h"

void sockdriver_process(MESSAGE* msg);

void sockdriver_wakeup_worker(struct worker_thread* wp);
struct worker_thread* sockdriver_worker(void);

#endif
