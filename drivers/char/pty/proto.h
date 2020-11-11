#ifndef _PTY_PROTO_H_
#define _PTY_PROTO_H_

struct tty* line2tty(dev_t line);
int in_process(struct tty* tty, char* buf, int count);
void out_process(struct tty* tty, char* bstart, char* bpos, char* bend,
                 int* icountp, int* ocountp);
void handle_events(struct tty* tty);
void tty_sigproc(struct tty* tty, int signo);

void do_pty(MESSAGE* msg);
void pty_init(struct tty* tty);
void select_retry_pty(struct tty* tty);

int devpts_clear(unsigned int index);
int devpts_set(unsigned int index, mode_t mode, uid_t uid, gid_t gid,
               dev_t dev);

#endif
