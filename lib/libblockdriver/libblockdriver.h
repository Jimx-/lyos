#ifndef _LIBBLOCKDRIVER_H_
#define _LIBBLOCKDRIVER_H_

struct blockdriver {
	int (*bdr_open)(dev_t minor, int access);
	int (*bdr_close)(dev_t minor);
	ssize_t (*bdr_readwrite)(dev_t minor, int do_write, u64 pos,
	  endpoint_t endpoint, char* buf, unsigned int count);
	int (*bdr_ioctl)(dev_t minor, int request, endpoint_t endpoint, char* buf);
	void (*bdr_intr)(unsigned mask);
	void (*bdr_alarm)(clock_t timestamp);
};

PUBLIC void blockdriver_process(struct blockdriver* bd, MESSAGE* msg);
PUBLIC void blockdriver_task(struct blockdriver* bd);

PUBLIC int announce_blockdev(char * name, dev_t dev);

#endif
