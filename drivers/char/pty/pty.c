/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */

#include <lyos/types.h>
#include <lyos/const.h>
#include <lyos/ipc.h>
#include <errno.h>
#include <lyos/sysutils.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <lyos/eventpoll.h>
#include <poll.h>

#include <libdevman/libdevman.h>
#include <libchardriver/libchardriver.h>

#include "tty.h"
#include "proto.h"

#define UNIX98_MODE (S_IFCHR | 0620)

#define UNIX98_MASTER(index) (UNIX98_MINOR + (index)*2)
#define UNIX98_SLAVE(index)  (UNIX98_MINOR + (index)*2 + 1)

struct pty {
    struct tty* tty;
    int state;

    int incaller;
    int inid;
    mgrant_id_t ingrant;
    int inleft;
    int incnt;

    int outcaller;
    int outid;
    mgrant_id_t outgrant;
    int outleft;
    int outcnt;

    int ocount;
    char *ohead, *otail;
    char obuf[TTY_OUT_BYTES];

    int select_ops;
    endpoint_t select_proc;
    dev_t select_minor;
};

#define PTY_ACTIVE 0x01
#define TTY_ACTIVE 0x02
#define PTY_CLOSED 0x04
#define TTY_CLOSED 0x08
#define PTY_UNIX98 0x10

static struct pty pty_table[NR_PTYS];

static int do_open(dev_t minor, int access, endpoint_t user_endpt);
static int do_close(dev_t minor);
static ssize_t do_read(dev_t minor, u64 pos, endpoint_t endpoint,
                       mgrant_id_t grant, unsigned int count, cdev_id_t id);
static ssize_t do_write(dev_t minor, u64 pos, endpoint_t endpoint,
                        mgrant_id_t grant, unsigned int count, cdev_id_t id);
static int do_ioctl(dev_t minor, int request, endpoint_t endpoint,
                    mgrant_id_t grant, endpoint_t user_endpoint, cdev_id_t id);
static int do_select(dev_t minor, int ops, endpoint_t endpoint);

static void pty_in_transfer(struct pty* pty);

/* Master-side operations */
static struct chardriver pty_driver = {
    .cdr_open = do_open,
    .cdr_close = do_close,
    .cdr_read = do_read,
    .cdr_write = do_write,
    .cdr_ioctl = do_ioctl,
    .cdr_select = do_select,
};

static struct tty* get_free_pty(void)
{
    struct tty* tty;
    struct pty* pty;

    for (tty = tty_table; tty < &tty_table[NR_PTYS]; tty++) {
        pty = tty->tty_dev;

        if (!(pty->state & (PTY_ACTIVE | TTY_ACTIVE))) return tty;
    }

    return NULL;
}

static int do_open(dev_t minor, int access, endpoint_t user_endpt)
{
    struct tty* tty;
    struct pty* pty;
    int retval;

    if (minor == PTMX_MINOR) {
        if ((tty = get_free_pty()) == NULL) return EAGAIN;

        if (devpts_clear(tty->tty_index)) return EAGAIN;

        pty = tty->tty_dev;
        pty->state |= PTY_UNIX98;

        minor = UNIX98_MASTER(tty->tty_index);

        retval = CDEV_CLONED | minor;
    } else {
        return EIO;
    }

    pty->state |= PTY_ACTIVE;

    pty->incnt = 0;
    pty->outcnt = 0;

    return retval;
}

static void pty_reset(struct tty* tty)
{
    struct pty* pty;

    pty = tty->tty_dev;

    if (pty->state & PTY_UNIX98) devpts_clear(tty->tty_index);

    pty->state = 0;
}

static int do_close(dev_t minor)
{
    struct tty* tty;
    struct pty* pty;

    tty = line2tty(minor);
    if (!tty) return ENXIO;

    pty = tty->tty_dev;

    if ((pty->state & (TTY_ACTIVE | TTY_CLOSED)) != TTY_ACTIVE)
        pty_reset(tty);
    else {
        pty->state |= PTY_CLOSED;
        tty->tty_termios.c_ospeed = B0;
        tty_sigproc(tty, SIGHUP);
    }

    return 0;
}

static ssize_t do_read(dev_t minor, u64 pos, endpoint_t endpoint,
                       mgrant_id_t grant, unsigned int count, cdev_id_t id)
{
    struct tty* tty = line2tty(minor);
    struct pty* pty;

    if (tty == NULL) {
        return -ENXIO;
    }

    pty = tty->tty_dev;

    if (pty->state & TTY_CLOSED) return 0;

    if (pty->incaller != NO_TASK || pty->inleft > 0 || pty->incnt > 0)
        return -EIO;
    if (count <= 0) return -EINVAL;

    /* tell the tty: */
    pty->incaller = endpoint; /* who wants the char */
    pty->inid = id;           /* task id */
    pty->ingrant = grant;     /* where the chars should be put */
    pty->inleft = count;      /* how many chars are requested */
    pty->incnt = 0;           /* how many chars have been transferred */

    pty_in_transfer(pty);

    handle_events(tty);

    if (pty->inleft == 0) {
        pty->incaller = NO_TASK;
        return SUSPEND; /* already done */
    }

    return SUSPEND;
}

static ssize_t do_write(dev_t minor, u64 pos, endpoint_t endpoint,
                        mgrant_id_t grant, unsigned int count, cdev_id_t id)
{
    struct tty* tty = line2tty(minor);
    struct pty* pty;

    if (tty == NULL) {
        return -ENXIO;
    }

    pty = tty->tty_dev;

    if (pty->state & TTY_CLOSED) return -EIO;

    if (pty->outcaller != NO_TASK || pty->outleft > 0 || pty->outcnt > 0)
        return -EIO;
    if (count <= 0) return -EINVAL;

    pty->outcaller = endpoint;
    pty->outid = id;
    pty->outgrant = grant;
    pty->outleft = count;
    pty->outcnt = 0;

    handle_events(tty);

    if (pty->outleft == 0) {
        pty->outcaller = NO_TASK;
        return SUSPEND; /* already done */
    }

    return SUSPEND;
}

static int do_ioctl(dev_t minor, int request, endpoint_t endpoint,
                    mgrant_id_t grant, endpoint_t user_endpoint, cdev_id_t id)
{
    struct tty* tty;
    struct pty* pty;
    uid_t uid;
    int retval;

    tty = line2tty(minor);
    if (!tty) return ENXIO;

    pty = tty->tty_dev;

    switch (request) {
    case TIOCGRANTPT:
        if (!(pty->state & PTY_UNIX98)) break;

        if (get_epinfo(user_endpoint, &uid, NULL) == -1) return EACCES;

        if (devpts_set(tty->tty_index, UNIX98_MODE, uid, 0,
                       MAKE_DEV(DEV_CHAR_PTY, UNIX98_SLAVE(tty->tty_index))) !=
            OK)
            return EACCES;

        return 0;
    case TIOCSPTLCK:
        return 0;
    case TIOCGPTN:
        retval = safecopy_to(endpoint, grant, 0, &tty->tty_index,
                             sizeof(unsigned int));

        return retval;
    default:
        break;
    }

    return EINVAL;
}

static int select_try_pty(struct tty* tty, int ops)
{
    struct pty* pty = tty->tty_dev;
    int ready_ops = 0;

    if (ops & EPOLLIN) {
        if (pty->state & TTY_CLOSED)
            ready_ops |= EPOLLIN;
        else if (pty->inleft != 0 || pty->incnt != 0)
            ready_ops |= EPOLLIN;
        else if (pty->ocount > 0)
            ready_ops |= EPOLLIN;
    }

    if (ops & EPOLLOUT) {
        if (pty->state & TTY_CLOSED)
            ready_ops |= EPOLLOUT;
        else if (pty->outleft != 0 || pty->outcnt != 0)
            ready_ops |= EPOLLOUT;
        else if (tty->tty_trans_cnt < buflen(tty->ibuf))
            ready_ops |= EPOLLOUT;
    }

    return ready_ops;
}

void select_retry_pty(struct tty* tty)
{
    struct pty* pty = tty->tty_dev;
    int ops;

    if (pty->select_ops && (ops = select_try_pty(tty, pty->select_ops))) {
        chardriver_poll_notify(pty->select_minor, ops);
        pty->select_ops &= ~ops;
    }
}

static int do_select(dev_t minor, int ops, endpoint_t endpoint)
{
    struct tty* tty = line2tty(minor);
    struct pty* pty;

    if (tty == NULL) {
        return -ENXIO;
    }

    pty = tty->tty_dev;

    int watch = ops & POLL_NOTIFY;
    ops &= (EPOLLIN | EPOLLOUT);

    int ready_ops = select_try_pty(tty, ops);

    ops &= ~ready_ops;
    if (ops && watch) {
        pty->select_ops |= ops;
        pty->select_proc = endpoint;
        pty->select_minor = minor;
    }

    return ready_ops;
}

void do_pty(MESSAGE* msg) { chardriver_process(&pty_driver, msg); }

static void pty_in_transfer(struct pty* pty)
{
    int count;

    for (;;) {
        count = bufend(pty->obuf) - pty->otail;
        if (count > pty->ocount) count = pty->ocount;
        if (count == 0 || pty->inleft == 0) break;

        if (count > pty->inleft) count = pty->inleft;
        if (!count) break;

        if (safecopy_to(pty->incaller, pty->ingrant, pty->incnt, pty->otail,
                        count) != OK)
            break;

        pty->ocount -= count;
        pty->otail += count;
        if (pty->otail == bufend(pty->obuf)) pty->otail = pty->obuf;
        pty->incnt += count;
        pty->inleft -= count;
    }
}

static void do_slave_read(struct tty* tty)
{
    struct pty* pty;
    char c;
    int retval;

    pty = tty->tty_dev;

    if (pty->state & PTY_CLOSED) {
        if (tty->tty_inleft > 0) {
            chardriver_reply_io(tty->tty_incaller, tty->tty_inid,
                                tty->tty_trans_cnt);
            tty->tty_inleft = tty->tty_trans_cnt = 0;
            tty->tty_incaller = NO_TASK;
        }
    }

    while (pty->outleft > 0) {
        retval =
            safecopy_from(pty->outcaller, pty->outgrant, pty->outcnt, &c, 1);
        if (retval) break;

        in_process(tty, &c, 1);

        pty->outcnt++;
        pty->outleft--;

        if (pty->outleft == 0) {
            chardriver_reply_io(pty->outcaller, pty->outid, pty->outcnt);
            pty->outcnt = 0;
            pty->outcaller = NO_TASK;
        }
    }
}

static void do_slave_write(struct tty* tty)
{
    struct pty* pty = tty->tty_dev;
    int count, ocount;
    int retval;

    if (pty->state & PTY_CLOSED) {
        if (tty->tty_outleft > 0) {
            chardriver_reply_io(tty->tty_outcaller, tty->tty_outid, -EIO);
            tty->tty_outcaller = NO_TASK;
            tty->tty_outleft = tty->tty_outcnt = 0;
        }
    }

    for (;;) {
        ocount = buflen(pty->obuf) - pty->ocount;
        count = bufend(pty->obuf) - pty->ohead;

        count = min(count, ocount);
        count = min(count, tty->tty_outleft);

        if (count == 0) break;

        retval = safecopy_from(tty->tty_outcaller, tty->tty_outgrant,
                               tty->tty_outcnt, pty->ohead, count);
        if (retval) break;

        out_process(tty, pty->obuf, pty->ohead, bufend(pty->obuf), &count,
                    &ocount);
        if (!count) break;

        pty->ocount = ocount;
        pty->ohead += ocount;
        if (pty->ohead >= bufend(pty->obuf)) pty->ohead -= buflen(pty->obuf);

        pty_in_transfer(pty);

        tty->tty_outcnt += count;
        tty->tty_outleft -= count;

        if (tty->tty_outleft == 0) {
            chardriver_reply_io(tty->tty_outcaller, tty->tty_outid,
                                tty->tty_outcnt);
            tty->tty_outcnt = 0;
            tty->tty_outcaller = NO_TASK;
        }
    }

    if (pty->incnt > 0) {
        chardriver_reply_io(pty->incaller, pty->inid, pty->incnt);
        pty->inleft = 0;
        pty->incnt = 0;
        pty->incaller = NO_TASK;
    }
}

static void do_slave_close(struct tty* tty)
{
    struct pty* pty = tty->tty_dev;

    if (!(pty->state & PTY_ACTIVE)) return;

    if (pty->inleft > 0) {
        chardriver_reply_io(pty->incaller, pty->inid, pty->incnt);
        pty->incnt = pty->inleft = 0;
        pty->incaller = NO_TASK;
    }

    if (pty->outleft > 0) {
        chardriver_reply_io(pty->outcaller, pty->outid, pty->outcnt);
        pty->outcnt = pty->outleft = 0;
        pty->outcaller = NO_TASK;
    }

    if (pty->state & PTY_CLOSED)
        pty_reset(tty);
    else
        pty->state |= TTY_CLOSED;
}

static void do_slave_echo(struct tty* tty, char c)
{
    struct pty* pty = tty->tty_dev;
    int count, ocount;

    ocount = buflen(pty->obuf) - pty->ocount;
    if (ocount == 0) return;
    count = 1;
    *pty->ohead = c;

    out_process(tty, pty->obuf, pty->ohead, bufend(pty->obuf), &count, &ocount);
    if (count == 0) return;

    pty->ocount += ocount;
    pty->ohead += ocount;
    if (pty->ohead >= bufend(pty->obuf)) pty->ohead -= buflen(pty->obuf);

    pty_in_transfer(pty);
}

void pty_init(struct tty* tty)
{
    struct pty* pty;
    int line;

    line = tty - tty_table;
    pty = tty->tty_dev = &pty_table[line];
    pty->tty = tty;
    pty->state = 0;

    pty->select_ops = 0;
    pty->incaller = NO_TASK;
    pty->outcaller = NO_TASK;

    pty->ohead = pty->otail = pty->obuf;

    tty->tty_devread = do_slave_read;
    tty->tty_devwrite = do_slave_write;
    tty->tty_echo = do_slave_echo;
    tty->tty_close = do_slave_close;
}
