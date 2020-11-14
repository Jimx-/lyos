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
#include <lyos/service.h>
#include <lyos/timer.h>
#include <lyos/sysutils.h>
#include <sys/ioctl.h>
#include <lyos/eventpoll.h>
#include <errno.h>
#include <string.h>
#include <poll.h>

#include <libdevman/libdevman.h>
#include <libchardriver/libchardriver.h>

#include "tty.h"
#include "proto.h"

struct tty tty_table[NR_PTYS];
#define tty_addr(line) (&tty_table[line])
#define TTY_FIRST      tty_addr(0)
#define TTY_END        tty_addr(sizeof(tty_table) / sizeof(tty_table[0]))

/* Default termios */
static struct termios termios_defaults = {
    TINPUT_DEF,
    TOUTPUT_DEF,
    TCTRL_DEF,
    TLOCAL_DEF,
    TSPEED_DEF,
    TSPEED_DEF,
    {
        TINTR_DEF,
        TQUIT_DEF,
        TERASE_DEF,
        TKILL_DEF,
        TEOF_DEF,
        TTIME_DEF,
        TMIN_DEF,
        0,
        TSTART_DEF,
        TSTOP_DEF,
        TSUSP_DEF,
        TEOL_DEF,
        TREPRINT_DEF,
        TDISCARD_DEF,
        0,
        TLNEXT_DEF,
    },
};

static class_id_t tty_subsys_id = NO_CLASS_ID;

static int do_open(dev_t minor, int access, endpoint_t user_endpt);
static int do_close(dev_t minor);
static ssize_t do_read(dev_t minor, u64 pos, endpoint_t endpoint,
                       mgrant_id_t grant, unsigned int count, int flags,
                       cdev_id_t id);
static ssize_t do_write(dev_t minor, u64 pos, endpoint_t endpoint,
                        mgrant_id_t grant, unsigned int count, int flags,
                        cdev_id_t id);
static int do_ioctl(dev_t minor, int request, endpoint_t endpoint,
                    mgrant_id_t grant, int flags, endpoint_t user_endpoint,
                    cdev_id_t id);
static int do_select(dev_t minor, int ops, endpoint_t endpoint);

static void in_transfer(struct tty* tty);
static int select_try(struct tty* tty, int ops);
static int select_retry(struct tty* tty);
static void tty_icancel(struct tty* tty);

static struct chardriver tty_driver = {
    .cdr_open = do_open,
    .cdr_close = do_close,
    .cdr_read = do_read,
    .cdr_write = do_write,
    .cdr_ioctl = tty_ioctl,
    .cdr_select = do_select,
};

static int is_pty(dev_t minor)
{
    if (minor == PTMX_MINOR) return TRUE;

    if (minor >= UNIX98_MINOR && minor < UNIX98_MINOR + NR_PTYS * 2 &&
        !(minor & 1))
        return TRUE;

    return FALSE;
}

static int do_open(dev_t minor, int access, endpoint_t user_endpt)
{
    struct tty* tty;
    int retval = 0;

    tty = line2tty(minor);
    if (!tty) return ENXIO;

    if (!(access & CDEV_NOCTTY)) {
        tty->tty_pgrp = user_endpt;
        retval = CDEV_CTTY;
    }

    return retval;
}

static int do_close(dev_t minor)
{
    struct tty* tty;

    tty = line2tty(minor);
    if (!tty) return ENXIO;

    tty->tty_pgrp = NO_TASK;
    tty_icancel(tty);
    tty->tty_close(tty);
    tty->tty_termios = termios_defaults;

    return 0;
}

static ssize_t do_read(dev_t minor, u64 pos, endpoint_t endpoint,
                       mgrant_id_t grant, unsigned int count, int flags,
                       cdev_id_t id)
{
    struct tty* tty = line2tty(minor);

    if (tty == NULL) {
        return -ENXIO;
    }

    if (tty->tty_incaller != NO_TASK || tty->tty_inleft > 0) return -EIO;
    if (count <= 0) return -EINVAL;

    /* tell the tty: */
    tty->tty_incaller = endpoint; /* who wants the char */
    tty->tty_inid = id;           /* task id */
    tty->tty_ingrant = grant;     /* where the chars should be put */
    tty->tty_inleft = count;      /* how many chars are requested */
    tty->tty_trans_cnt = 0;       /* how many chars have been transferred */

    in_transfer(tty);
    handle_events(tty);

    if (tty->tty_inleft == 0) return SUSPEND; /* already done */

    if (tty->tty_select_ops) {
        select_retry(tty);
    }

    return SUSPEND;
}

static ssize_t do_write(dev_t minor, u64 pos, endpoint_t endpoint,
                        mgrant_id_t grant, unsigned int count, int flags,
                        cdev_id_t id)
{
    struct tty* tty = line2tty(minor);

    if (tty == NULL) return -ENXIO;

    if (tty->tty_outcaller != NO_TASK || tty->tty_outleft > 0) return -EIO;
    if (count <= 0) return -EINVAL;

    tty->tty_outcaller = endpoint;
    tty->tty_outid = id;
    tty->tty_outgrant = grant;
    tty->tty_outleft = count;
    tty->tty_outcnt = 0;

    handle_events(tty);

    if (tty->tty_outleft == 0) return SUSPEND; /* already done */

    if (tty->tty_select_ops) {
        select_retry(tty);
    }

    return SUSPEND;
}

void tty_sigproc(struct tty* tty, int signo)
{
    endpoint_t ep;
    int retval;

    if (get_procep(tty->tty_pgrp, &ep) != 0) return;

    if ((retval = kernel_kill(ep, signo)) != 0)
        panic("unable to send signal(%d)", retval);
}

int tty_ioctl(dev_t minor, int request, endpoint_t endpoint, mgrant_id_t grant,
              int flags, endpoint_t user_endpoint, cdev_id_t id)
{
    int retval = 0;
    int i;
    struct tty* tty = line2tty(minor);

    if (tty == NULL) {
        return ENXIO;
    }

    switch (request) {
    case TCGETS: /* get termios attributes */
        retval = safecopy_to(endpoint, grant, 0, &tty->tty_termios,
                             sizeof(struct termios));
        break;
    case TCSETSW:
    case TCSETSF:
        if (tty->tty_outleft > 0) {
            /* wait for output process to finish */
            tty->tty_iocaller = endpoint;
            tty->tty_iogrant = grant;
            tty->tty_ioid = id;
            tty->tty_ioreq = request;
            return SUSPEND;
        }
        if (request == TCSETSF) tty_icancel(tty);
        /* fall through */
    case TCSETS: /* set termios attributes */
        retval = safecopy_from(endpoint, grant, 0, &tty->tty_termios,
                               sizeof(struct termios));
        if (retval != 0) break;
        break;
    case TCFLSH:
        retval = safecopy_from(endpoint, grant, 0, &i, sizeof(i));
        if (retval != 0) break;
        if (i == TCIFLUSH || i == TCIOFLUSH) {
            tty_icancel(tty);
        }
        if (i == TCOFLUSH || i == TCIOFLUSH) {
        }
        break;
    case TIOCGPGRP: /* get/set process group */
        retval = safecopy_to(endpoint, grant, 0, &tty->tty_pgrp,
                             sizeof(tty->tty_pgrp));
        break;
    case TIOCSPGRP:
        retval = safecopy_from(endpoint, grant, 0, &tty->tty_pgrp,
                               sizeof(tty->tty_pgrp));
        break;
    case TIOCGWINSZ:
        retval = safecopy_to(endpoint, grant, 0, &tty->tty_winsize,
                             sizeof(struct winsize));
        break;
    case TIOCSWINSZ:
        retval = safecopy_from(endpoint, grant, 0, &tty->tty_winsize,
                               sizeof(struct winsize));
        tty_sigproc(tty, SIGWINCH);
        break;
    default:
        retval = EINVAL;
        break;
    }

    return retval;
}

static int do_select(dev_t minor, int ops, endpoint_t endpoint)
{
    struct tty* tty = line2tty(minor);

    if (tty == NULL) {
        return -ENXIO;
    }

    int watch = ops & POLL_NOTIFY;
    ops &= (EPOLLIN | EPOLLOUT);

    int ready_ops = select_try(tty, ops);

    ops &= ~ready_ops;
    if (ops && watch) {
        if (tty->tty_select_ops != 0 && tty->tty_select_minor != minor) {
            ready_ops = -EBADF;
        } else {
            tty->tty_select_ops |= ops;
            tty->tty_select_proc = endpoint;
            tty->tty_select_minor = minor;
        }
    }

    return ready_ops;
}

static void tty_icancel(struct tty* tty)
{
    tty->tty_trans_cnt = tty->tty_eotcnt = 0;
    tty->ibuf_tail = tty->ibuf_head;
}

static void tty_dev_ioctl(struct tty* tty)
{
    int retval = 0;
    if (tty->tty_outleft > 0) return;

    if (tty->tty_ioreq == TCSETSF) tty_icancel(tty);
    retval = safecopy_from(tty->tty_iocaller, tty->tty_iogrant, 0,
                           &tty->tty_termios, sizeof(struct termios));
    tty->tty_ioreq = 0;
    chardriver_reply_io(tty->tty_iocaller, tty->tty_ioid, retval);
    tty->tty_iocaller = NO_TASK;
}

static void in_transfer(struct tty* tty)
{
    char buf[64], *bp;

    if (tty->tty_inleft == 0 || tty->tty_eotcnt < tty->tty_min) return;

    bp = buf;
    while (tty->tty_inleft > 0 && tty->tty_eotcnt > 0) {
        int ch = *(tty->ibuf_tail);

        if (++tty->ibuf_tail == bufend(tty->ibuf)) tty->ibuf_tail = tty->ibuf;
        tty->ibuf_cnt--;

        *bp = ch & IN_CHAR;
        tty->tty_inleft--;
        if (++bp == buf + sizeof(buf)) { /* buffer full */
            safecopy_to(tty->tty_incaller, tty->tty_ingrant, tty->tty_trans_cnt,
                        buf, sizeof(buf));
            tty->tty_trans_cnt += sizeof(buf);
            bp = buf;
        }

        if (ch & IN_EOT) {
            tty->tty_eotcnt--;
            if (tty->tty_termios.c_lflag & ICANON) tty->tty_inleft = 0;
        }
    }

    if (bp > buf) {
        int count = bp - buf;
        safecopy_to(tty->tty_incaller, tty->tty_ingrant, tty->tty_trans_cnt,
                    buf, count);
        tty->tty_trans_cnt += count;
    }

    if (tty->tty_inleft == 0) {
        chardriver_reply_io(tty->tty_incaller, tty->tty_inid,
                            tty->tty_trans_cnt);
        tty->tty_incaller = NO_TASK;
        tty->tty_trans_cnt = 0;
    }
}

static int select_try(struct tty* tty, int ops)
{
    int ready_ops = 0;

    if (tty->tty_termios.c_ospeed == B0) {
        ready_ops |= ops;
    }

    if (ops & EPOLLIN) {
        if (tty->tty_inleft > 0) {
            ready_ops |= EPOLLIN;
        } else if (tty->ibuf_cnt > 0) {
            if (!(tty->tty_termios.c_lflag & ICANON) || tty->tty_eotcnt > 0) {
                ready_ops |= EPOLLIN;
            }
        }
    }

    if (ops & EPOLLOUT) {
        if (tty->tty_outleft > 0) {
            ready_ops |= EPOLLOUT;
        }
    }

    return ready_ops;
}

static int select_retry(struct tty* tty)
{
    int ops;
    if (tty->tty_select_ops && (ops = select_try(tty, tty->tty_select_ops))) {
        chardriver_poll_notify(tty->tty_select_minor, ops);
        tty->tty_select_ops &= ~ops;
    }

    return 0;
}

void handle_events(struct tty* tty)
{
    do {
        tty->tty_events = 0;

        tty->tty_devread(tty);

        tty->tty_devwrite(tty);

        if (tty->tty_ioreq) tty_dev_ioctl(tty);
    } while (tty->tty_events);

    in_transfer(tty);

    /* return to caller if bytes are available */
    if (tty->tty_trans_cnt >= tty->tty_min && tty->tty_inleft > 0) {
        chardriver_reply_io(tty->tty_incaller, tty->tty_inid,
                            tty->tty_trans_cnt);
        tty->tty_inleft = tty->tty_trans_cnt = 0;
        tty->tty_incaller = NO_TASK;
    }

    if (tty->tty_select_ops) {
        select_retry(tty);
    }
    select_retry_pty(tty);
}

struct tty* line2tty(dev_t line)
{
    struct tty* tp;

    if ((line - UNIX98_MINOR) < NR_PTYS * 2) {
        tp = tty_addr((line - UNIX98_MINOR) >> 1);
    } else {
        tp = NULL;
    }

    return tp;
}

static void erase(struct tty* tty)
{
    if (tty->ibuf_cnt == 0) return;

    u32* head = tty->ibuf_head;
    if (head == tty->ibuf) head = tty->ibuf + TTY_IN_BYTES;
    head--;
    tty->ibuf_head = head;
    tty->ibuf_cnt--;

    if (tty->tty_termios.c_lflag & ECHOE) {
        tty->tty_echo(tty, '\b');
    }
}

static void tty_echo(struct tty* tty, char c)
{
    int len;
    if ((c & IN_CHAR) < ' ') {
        switch (c & (IN_CHAR)) {
        case '\t':
            len = 0;
            do {
                tty->tty_echo(tty, ' ');
                len++;
            } while (len < TAB_SIZE);
            break;
        case '\n':
        case '\r':
            tty->tty_echo(tty, c);
            break;
        default:
            tty->tty_echo(tty, '^');
            tty->tty_echo(tty, '@' + (c & IN_CHAR));
            break;
        }
    } else {
        tty->tty_echo(tty, c);
    }
}

int in_process(struct tty* tty, char* buf, int count)
{
    int cnt, key;

    for (cnt = 0; cnt < count; cnt++) {
        key = *buf++ & 0xFF;

        if (tty->tty_termios.c_iflag & ISTRIP) key &= 0x7F;

        if (tty->tty_termios.c_lflag & IEXTEN) {
            /* Previous character was a character escape? */
            if (tty->tty_escaped) {
                tty->tty_escaped = 0;
                key |= IN_ESC;
            }

            /* LNEXT (^V) to escape the next character? */
            if (key == tty->tty_termios.c_cc[VLNEXT]) {
                tty->tty_escaped = 1;
                tty_echo(tty, '^');
                tty_echo(tty, '\b');
                continue;
            }
        }

        /* Map CR to LF, ignore CR, or map LF to CR. */
        if (key == '\r') {
            if (tty->tty_termios.c_iflag & IGNCR) return cnt;
            if (tty->tty_termios.c_iflag & ICRNL) key = '\n';
        } else if (key == '\n') {
            if (tty->tty_termios.c_iflag & INLCR) key = '\r';
        }

        if (tty->tty_termios.c_lflag & ICANON) {
            if (key == tty->tty_termios.c_cc[VERASE]) {
                erase(tty);
                if (!(tty->tty_termios.c_lflag & ECHOE)) {
                    tty_echo(tty, key);
                }
                continue;
            }

            if (key == tty->tty_termios.c_cc[VEOF]) {
                key |= IN_EOT | IN_EOF;
            }

            if (key == '\n') key |= IN_EOT;

            if (key == tty->tty_termios.c_cc[VEOL]) key |= IN_EOT;
        }

        if (tty->tty_termios.c_lflag & ISIG) {
            int signo = 0;

            if (key == tty->tty_termios.c_cc[VINTR]) {
                signo = SIGINT;
            } else if (key == tty->tty_termios.c_cc[VQUIT]) {
                signo = SIGQUIT;
            }

            if (signo > 0) {
                tty_echo(tty, key);
                tty_sigproc(tty, signo);
                continue;
            }
        }

        if (tty->ibuf_cnt == buflen(tty->ibuf)) {
            if (tty->tty_termios.c_lflag & ICANON) continue;
            break;
        }

        if (!(tty->tty_termios.c_lflag & ICANON)) {
            /* all characters are line break in raw mode */
            key |= IN_EOT;
        }

        if (tty->tty_termios.c_lflag & (ECHO | ECHONL)) tty_echo(tty, key);

        *tty->ibuf_head++ = key;
        if (tty->ibuf_head == bufend(tty->ibuf)) {
            tty->ibuf_head = tty->ibuf;
        }
        tty->ibuf_cnt++;
        if (key & IN_EOT) tty->tty_eotcnt++;

        if (tty->ibuf_cnt == buflen(tty->ibuf)) in_transfer(tty);
    }

    return cnt;
}

void out_process(struct tty* tty, char* bstart, char* bpos, char* bend,
                 int* icountp, int* ocountp)
{
    int icount = *icountp;
    int ocount = *ocountp;
    int pos = tty->tty_pos;
    int tablen;

    while (icount > 0) {
        switch (*bpos) {
        case '\7':
            break;
        case '\b':
            pos--;
            break;
        case '\r':
            pos = 0;
            break;
        case '\n':
            if ((tty->tty_termios.c_oflag & (OPOST | ONLCR)) ==
                (OPOST | ONLCR)) {
                if (ocount >= 2) {
                    *bpos = '\r';
                    if (++bpos == bend) bpos = bstart;
                    *bpos = '\n';
                    pos = 0;
                    icount--;
                    ocount -= 2;
                }
                goto done;
            }
            break;
        case '\t':
            tablen = TAB_SIZE - (pos & TAB_MASK);

            if ((tty->tty_termios.c_oflag & (OPOST | XTABS)) ==
                (OPOST | XTABS)) {
                if (ocount >= tablen) {
                    pos += tablen;
                    icount--;
                    ocount -= tablen;
                    do {
                        *bpos = ' ';
                        if (++bpos == bend) bpos = bstart;
                    } while (--tablen != 0);
                }
                goto done;
            }

            pos += tablen;
            break;
        default:
            pos++;
            break;
        }

        if (++bpos == bend) bpos = bstart;
        icount--;
        ocount--;
    }

done:
    tty->tty_pos = pos & TAB_MASK;

    *icountp -= icount;
    *ocountp -= ocount;
}

static int init_tty(void)
{
    struct tty* tty;
    dev_t devt;
    struct device_info devinf;
    device_id_t device_id;
    int i, retval;

    printl("pty: pseudo terminal driver is running.\n");

    if ((retval = dm_class_get_or_create("tty", &tty_subsys_id)) != OK)
        return retval;

    /* add /dev/ptmx */
    devt = MAKE_DEV(DEV_CHAR_PTY, PTMX_MINOR);
    dm_cdev_add(devt);

    memset(&devinf, 0, sizeof(devinf));
    strlcpy(devinf.name, "ptmx", sizeof(devinf.name));
    devinf.bus = NO_BUS_ID;
    devinf.class = tty_subsys_id;
    devinf.parent = NO_DEVICE_ID;
    devinf.devt = devt;
    devinf.type = DT_CHARDEV;

    retval = dm_device_register(&devinf, &device_id);
    if (retval) return retval;

    for (tty = TTY_FIRST, i = 0; tty < TTY_END; tty++, i++) {
        memset(tty, 0, sizeof(*tty));

        tty->tty_index = i;

        tty->ibuf_cnt = tty->tty_eotcnt = 0;
        tty->ibuf_head = tty->ibuf_tail = tty->ibuf;
        tty->tty_min = 1;
        tty->tty_termios = termios_defaults;

        tty->tty_incaller = NO_TASK;
        tty->tty_outcaller = NO_TASK;
        tty->tty_inleft = 0;
        tty->tty_outleft = 0;
        tty->tty_iocaller = NO_TASK;
        tty->tty_ioreq = 0;

        pty_init(tty);
    }

    return 0;
}

int main()
{
    struct tty* tty;
    MESSAGE msg;
    dev_t minor;
    endpoint_t src;

    serv_register_init_fresh_callback(init_tty);
    serv_init();

    while (1) {
        for (tty = TTY_FIRST; tty < TTY_END; tty++) {
            if (tty->tty_events) handle_events(tty);
        }

        send_recv(RECEIVE_ASYNC, ANY, &msg);

        src = msg.source;

        /* notify */
        if (msg.type == NOTIFY_MSG) {
            switch (src) {
            case CLOCK:
                expire_timer(msg.TIMESTAMP);
                break;
            }
            continue;
        }

        if (chardriver_get_minor(&msg, &minor) != OK) continue;

        if (is_pty(minor)) {
            do_pty(&msg);
            continue;
        }

        chardriver_process(&tty_driver, &msg);
    }

    return 0;
}
