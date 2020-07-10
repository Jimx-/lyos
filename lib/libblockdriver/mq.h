#ifndef _LIBBLOCKDRIVER_MQ_H_
#define _LIBBLOCKDRIVER_MQ_H_

#include <lyos/types.h>
#include <lyos/ipc.h>

void mq_init(void);
int mq_empty(void);
int mq_enqueue(const MESSAGE* msg);
int mq_dequeue(MESSAGE* msg);

#endif
