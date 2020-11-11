#ifndef _PTY_TTY_H_
#define _PTY_TTY_H_

#include <lyos/config.h>
#include <sys/types.h>
#include <lyos/types.h>
#include <termios.h>

#define PTMX_MINOR   0
#define UNIX98_MINOR 128

#define TTY_IN_BYTES  256  /* tty input queue size */
#define TTY_OUT_BYTES 2048 /* tty output queue size */

struct tty;

typedef void (*devfun_t)(struct tty* tp);
typedef void (*devfunarg_t)(struct tty* tp, char c);

struct tty {
    int tty_index;

    u32 ibuf[TTY_IN_BYTES]; /* TTY input buffer */
    u32* ibuf_head;         /* the next free slot */
    u32* ibuf_tail;         /* the val to be processed by TTY */
    int ibuf_cnt;           /* how many */

    int tty_events;
    int tty_incaller;
    int tty_inid;
    mgrant_id_t tty_ingrant;
    int tty_inleft;
    int tty_trans_cnt;

    int tty_outcaller;
    int tty_outid;
    mgrant_id_t tty_outgrant;
    int tty_outleft;
    int tty_outcnt;

    int tty_iocaller;
    int tty_ioid;
    int tty_ioreq;
    mgrant_id_t tty_iogrant;

    endpoint_t tty_pgrp;
    int tty_min;
    int tty_eotcnt;

    int tty_select_ops;
    endpoint_t tty_select_proc;
    dev_t tty_select_minor;

    struct termios tty_termios;
    struct winsize tty_winsize;

    int tty_pos;
    int tty_escaped;

    void* tty_dev;
    devfun_t tty_devread;
    devfun_t tty_devwrite;
    devfunarg_t tty_echo;
    devfun_t tty_close;
};

extern struct tty tty_table[NR_PTYS];

#define buflen(buf) (sizeof(buf) / sizeof((buf)[0]))
#define bufend(buf) ((buf) + buflen(buf))

#define IN_CHAR 0x00FF /* low 8 bits */
#define IN_EOT  0x1000 /* char is line break */
#define IN_EOF  0x2000 /* char is EOF */
#define IN_ESC  0x4000 /* escaped by ^V */

#define TAB_SIZE 8
#define TAB_MASK 7

#endif
