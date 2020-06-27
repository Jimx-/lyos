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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include <stdlib.h>
#include "unistd.h"
#include "assert.h"
#include "errno.h"
#include "fcntl.h"
#include <sys/ioctl.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "tty.h"
#include "console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "termios.h"
#include "proto.h"
#include <lyos/driver.h>
#include <lyos/param.h>
#include <lyos/log.h>
#include <lyos/sysutils.h>
#include <lyos/timer.h>
#include "global.h"

#include <libdevman/libdevman.h>
#include <libchardriver/libchardriver.h>

static struct sysinfo* _sysinfo;
static dev_t cons_minor = CONS_MINOR + 1;

static bus_type_id_t tty_subsys_id;

#define TTY_FIRST (tty_table)
#define TTY_END (tty_table + NR_CONSOLES + NR_SERIALS)

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

static void init_tty();
static TTY* minor2tty(dev_t minor);
static void set_console_line(char val[CONS_ARG]);
static void tty_dev_read(TTY* tty);
static void tty_dev_write(TTY* tty);
static void tty_dev_ioctl(TTY* tty);
static void in_transfer(TTY* tty);
static int do_open(dev_t minor, int access);
static ssize_t do_read(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                       unsigned int count, cdev_id_t id);
static ssize_t do_write(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                        unsigned int count, cdev_id_t id);
static int do_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                    cdev_id_t id);
static int do_select(dev_t minor, int ops, endpoint_t endpoint);
static void tty_do_kern_log();
static void tty_echo(TTY* tty, char c);
static void tty_sigproc(TTY* tty, int signo);
static void erase(TTY* tty);
static int select_try(TTY* tty, int ops);
static int select_retry(TTY* tty);
static void tty_icancel(TTY* tty);

static struct chardriver tty_driver = {
    .cdr_open = do_open,
    .cdr_read = do_read,
    .cdr_write = do_write,
    .cdr_ioctl = do_ioctl,
    .cdr_select = do_select,
};

/*****************************************************************************
 *                                task_tty
 *****************************************************************************/
/**
 * <Ring 1> Main loop of task TTY.
 *****************************************************************************/
int main()
{
    TTY* tty;
    MESSAGE msg;

    init_tty();

    while (1) {
        for (tty = TTY_FIRST; tty < TTY_END; tty++) {
            handle_events(tty);
        }

        send_recv(RECEIVE_ASYNC, ANY, &msg);

        int src = msg.source;
        assert(src != TASK_TTY);

        /* notify */
        if (msg.type == NOTIFY_MSG) {
            switch (msg.source) {
            case KERNEL:
                tty_do_kern_log();
                break;
            case INTERRUPT:
                if (msg.INTERRUPTS & rs_irq_set) rs_interrupt(&msg);
                break;
            case CLOCK:
                expire_timer(msg.TIMESTAMP);
                break;
            }
            continue;
        }

        switch (msg.type) {
        case INPUT_TTY_UP:
        case INPUT_TTY_EVENT:
            do_input(&msg);
            continue;
        default:
            break;
        }

        chardriver_process(&tty_driver, &msg);
    }

    return 0;
}

/*****************************************************************************
 *                                init_tty
 *****************************************************************************/
/**
 * Things to be initialized before a tty can be used:
 *   -# the input buffer
 *   -# the corresponding console
 *
 *****************************************************************************/
static void init_tty()
{
    TTY* tty;
    int i;
    struct device_info devinf;
    device_id_t device_id;
    dev_t devt;

    get_sysinfo(&_sysinfo);

    char val[CONS_ARG];
    if (env_get_param("console", val, CONS_ARG) == 0) {
        set_console_line(val);
    }

    tty_subsys_id = dm_bus_register("tty");
    if (tty_subsys_id == BUS_TYPE_ERROR)
        panic("tty: cannot register tty subsystem");

    init_keyboard();

    /* add /dev/console */
    devt = MAKE_DEV(DEV_CHAR_TTY, CONS_MINOR);
    dm_cdev_add(devt);

    memset(&devinf, 0, sizeof(devinf));
    strlcpy(devinf.name, "console", sizeof(devinf.name));
    devinf.bus = tty_subsys_id;
    devinf.parent = NO_DEVICE_ID;
    devinf.devt = devt;
    devinf.type = DT_CHARDEV;

    device_id = dm_device_register(&devinf);
    if (device_id == NO_DEVICE_ID) panic("tty: cannot register console device");

    for (tty = TTY_FIRST, i = 0; tty < TTY_END; tty++, i++) {
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

        memset(&devinf, 0, sizeof(devinf));
        devinf.bus = tty_subsys_id;
        devinf.parent = NO_DEVICE_ID;
        devinf.type = DT_CHARDEV;

        if (i < NR_CONSOLES) { /* consoles */
            init_screen(tty);
            kb_init(tty);
            devt = MAKE_DEV(DEV_CHAR_TTY, (tty - TTY_FIRST + 1));
            dm_cdev_add(devt);

            snprintf(devinf.name, sizeof(devinf.name), "tty%d", i + 1);
            devinf.devt = devt;

            tty->tty_device_id = dm_device_register(&devinf);
            if (tty->tty_device_id == NO_DEVICE_ID)
                panic("tty: cannot register tty device");
        } else { /* serial ports */
            init_rs(tty);

            devt = MAKE_DEV(DEV_CHAR_TTY, i - NR_CONSOLES + SERIAL_MINOR);
            dm_cdev_add(devt);

            snprintf(devinf.name, sizeof(devinf.name), "ttyS%d",
                     i - NR_CONSOLES);
            devinf.devt = devt;

            tty->tty_device_id = dm_device_register(&devinf);
            if (tty->tty_device_id == NO_DEVICE_ID)
                panic("tty: cannot register tty device");
        }

        tty->tty_select_ops = 0;
        tty->tty_select_proc = NO_TASK;
        tty->tty_select_minor = NO_DEV;
    }

    select_console(0);

    /* start to handle kernel logging request */
    kernlog_register();
}

static TTY* minor2tty(dev_t minor)
{
    TTY* ptty = NULL;

    if (minor == CONS_MINOR || minor == LOG_MINOR) minor = cons_minor;

    if (minor - CONS_MINOR - 1 < NR_CONSOLES)
        ptty = &tty_table[minor - CONS_MINOR - 1];
    else if (minor - SERIAL_MINOR < NR_SERIALS)
        ptty = &tty_table[NR_CONSOLES + minor - SERIAL_MINOR];

    return ptty;
}

static void set_console_line(char val[CONS_ARG])
{
    if (!strncmp(val, "console", CONS_ARG - 1)) cons_minor = CONS_MINOR;

    char* pv = val;
    if (!strncmp(pv, "tty", 3)) {
        pv += 3;
        if (*pv == 'S') { /* serial */
            pv++;
            if (*pv >= '0' && *pv <= '9') {
                int minor = atoi(pv);
                if (minor <= NR_SERIALS) cons_minor = minor + SERIAL_MINOR;
            }
        } else if (*pv >= '0' && *pv <= '9') { /* console */
            int minor = atoi(pv);
            if (minor <= NR_CONSOLES) cons_minor = minor;
        }
    }
}

/*****************************************************************************
 *                                in_process
 *****************************************************************************/
/**
 * keyboard_read() will invoke this routine after having recognized a key press.
 *
 * @param tty  The key press is for whom.
 * @param key  The integer key with metadata.
 *****************************************************************************/
int in_process(TTY* tty, char* buf, int count)
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

/*****************************************************************************
 *                                tty_dev_read
 *****************************************************************************/
/**
 * Get chars from the keyboard buffer if the TTY::console is the `current'
 * console.
 *
 * @see keyboard_read()
 *
 * @param tty  Ptr to TTY.
 *****************************************************************************/
static void tty_dev_read(TTY* tty)
{
    if (tty->tty_devread) tty->tty_devread(tty);
}

/*****************************************************************************
 *                                tty_dev_write
 *****************************************************************************/
/**
 * Echo the char just pressed and transfer it to the waiting process.
 *
 * @param tty   Ptr to a TTY struct.
 *****************************************************************************/
static void tty_dev_write(TTY* tty)
{
    if (tty->tty_devwrite) tty->tty_devwrite(tty);
}

/*****************************************************************************
 *                                tty_dev_ioctl
 *****************************************************************************/
/**
 * Wait for output process to finish and execute the ioctl.
 *
 * @param tty   Ptr to a TTY struct.
 *****************************************************************************/
static void tty_dev_ioctl(TTY* tty)
{
    int retval = 0;
    if (tty->tty_outleft > 0) return;

    if (tty->tty_ioreq == TCSETSF) tty_icancel(tty);
    retval = data_copy(SELF, &(tty->tty_termios), tty->tty_iocaller,
                       tty->tty_iobuf, sizeof(struct termios));
    tty->tty_ioreq = 0;
    chardriver_reply_io(TASK_FS, tty->tty_ioid, retval);
    tty->tty_iocaller = NO_TASK;
}

/*****************************************************************************
 *                                in_transfer
 *****************************************************************************/
/**
 * Transfer chars to the waiting process.
 *
 * @param tty   Ptr to a TTY struct.
 *****************************************************************************/
static void in_transfer(TTY* tty)
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
            void* p = tty->tty_inbuf + tty->tty_trans_cnt;
            data_copy(tty->tty_incaller, p, TASK_TTY, buf, sizeof(buf));
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
        void* p = tty->tty_inbuf + tty->tty_trans_cnt;
        data_copy(tty->tty_incaller, p, TASK_TTY, buf, count);
        tty->tty_trans_cnt += count;
    }

    if (tty->tty_inleft == 0) {
        chardriver_reply_io(TASK_FS, tty->tty_inid, tty->tty_trans_cnt);
        tty->tty_incaller = NO_TASK;
        tty->tty_trans_cnt = 0;
    }
}

/*****************************************************************************
 *                                handle_events
 *****************************************************************************/
/**
 * Handle all events pending on a tty.
 *
 * @param tty   Ptr to a TTY struct.
 *****************************************************************************/
void handle_events(TTY* tty)
{
    do {
        tty->tty_events = 0;

        tty_dev_read(tty);
        tty_dev_write(tty);
        if (tty->tty_ioreq) tty_dev_ioctl(tty);
    } while (tty->tty_events);
    in_transfer(tty);

    /* return to caller if bytes are available */
    if (tty->tty_trans_cnt >= tty->tty_min && tty->tty_inleft > 0) {
        chardriver_reply_io(TASK_FS, tty->tty_inid, tty->tty_trans_cnt);
        tty->tty_inleft = tty->tty_trans_cnt = 0;
        tty->tty_incaller = NO_TASK;
    }

    if (tty->tty_select_ops) {
        select_retry(tty);
    }
}

static int do_open(dev_t minor, int access)
{
    TTY* tty = minor2tty(minor);

    if (tty == NULL) {
        return ENXIO;
    }

    return 0;
}

/*****************************************************************************
 *                                tty_do_read
 *****************************************************************************/
/**
 * Invoked when task TTY receives DEV_READ message.
 *
 * @note The routine will return immediately after setting some members of
 * TTY struct, telling FS to suspend the proc who wants to read. The real
 * transfer (tty buffer -> proc buffer) is not done here.
 *
 * @param tty  From which TTY the caller proc wants to read.
 * @param msg  The MESSAGE just received.
 *
 * @see documentation/tty/
 *****************************************************************************/
static ssize_t do_read(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                       unsigned int count, cdev_id_t id)
{
    TTY* tty = minor2tty(minor);

    if (tty == NULL) {
        return -ENXIO;
    }

    if (tty->tty_incaller != NO_TASK || tty->tty_inleft > 0) return -EIO;
    if (count <= 0) return -EINVAL;

    /* tell the tty: */
    tty->tty_incaller = endpoint; /* who wants the char */
    tty->tty_inid = id;           /* task id */
    tty->tty_inbuf = buf;         /* where the chars should be put */
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

/*****************************************************************************
 *                                tty_do_write
 *****************************************************************************/
/**
 * Invoked when task TTY receives DEV_WRITE message.
 *
 * @param tty  To which TTY the calller proc is bound.
 * @param msg  The MESSAGE.
 *****************************************************************************/
static ssize_t do_write(dev_t minor, u64 pos, endpoint_t endpoint, char* buf,
                        unsigned int count, cdev_id_t id)
{
    TTY* tty = minor2tty(minor);

    if (tty == NULL) {
        return -ENXIO;
    }
    if (tty->tty_outcaller != NO_TASK || tty->tty_outleft > 0) return -EIO;
    if (count <= 0) return -EINVAL;

    tty->tty_outcaller = endpoint;
    tty->tty_outid = id;
    tty->tty_outbuf = buf;
    tty->tty_outleft = count;
    tty->tty_outcnt = 0;

    handle_events(tty);

    if (tty->tty_outleft == 0) return SUSPEND; /* already done */

    if (tty->tty_select_ops) {
        select_retry(tty);
    }

    return SUSPEND;
}

/*****************************************************************************
 *                                tty_do_ioctl
 *****************************************************************************/
/**
 * Invoked when task TTY receives DEV_IOCTL message.
 *
 * @param tty  To which TTY the calller proc is bound.
 * @param msg  The MESSAGE.
 *****************************************************************************/
static int do_ioctl(dev_t minor, int request, endpoint_t endpoint, char* buf,
                    cdev_id_t id)
{
    int retval = 0;
    int i;
    TTY* tty = minor2tty(minor);

    if (tty == NULL) {
        return -ENXIO;
    }

    switch (request) {
    case TCGETS: /* get termios attributes */
        retval = data_copy(endpoint, buf, SELF, &(tty->tty_termios),
                           sizeof(struct termios));
        break;
    case TCSETSW:
    case TCSETSF:
        if (tty->tty_outleft > 0) {
            /* wait for output process to finish */
            tty->tty_iocaller = endpoint;
            tty->tty_iobuf = buf;
            tty->tty_ioid = id;
            tty->tty_ioreq = request;
            return SUSPEND;
        }
        if (request == TCSETSF) tty_icancel(tty);
    case TCSETS: /* set termios attributes */
        retval = data_copy(SELF, &(tty->tty_termios), endpoint, buf,
                           sizeof(struct termios));
        if (retval != 0) break;
        break;
    case TCFLSH:
        retval = data_copy(SELF, &i, endpoint, buf, sizeof(i));
        if (retval != 0) break;
        if (i == TCIFLUSH || i == TCIOFLUSH) {
            tty_icancel(tty);
        }
        if (i == TCOFLUSH || i == TCIOFLUSH) {
        }
        break;
    case TIOCGPGRP: /* get/set process group */
        retval = data_copy(endpoint, buf, SELF, &tty->tty_pgrp,
                           sizeof(tty->tty_pgrp));
        break;
    case TIOCSPGRP:
        retval = data_copy(SELF, &tty->tty_pgrp, endpoint, buf,
                           sizeof(tty->tty_pgrp));
        break;
    default:
        retval = EINVAL;
        break;
    }

    return retval;
}

static int select_try(TTY* tty, int ops)
{
    int ready_ops = 0;

    if (tty->tty_termios.c_ospeed == B0) {
        ready_ops |= ops;
    }

    if (ops & CDEV_SEL_RD) {
        if (tty->tty_inleft > 0) {
            ready_ops |= CDEV_SEL_RD;
        } else if (tty->ibuf_cnt > 0) {
            if (!(tty->tty_termios.c_lflag & ICANON) || tty->tty_eotcnt > 0) {
                ready_ops |= CDEV_SEL_RD;
            }
        }
    }

    if (ops & CDEV_SEL_WR) {
        if (tty->tty_outleft > 0) {
            ready_ops |= CDEV_SEL_WR;
        }
    }

    return ready_ops;
}

static int select_retry(TTY* tty)
{
    int ops;
    if (tty->tty_select_ops && (ops = select_try(tty, tty->tty_select_ops))) {
        MESSAGE msg;
        memset(&msg, 0, sizeof(MESSAGE));
        msg.type = CDEV_SELECT_REPLY2;
        msg.RETVAL = ops;
        msg.DEVICE = tty->tty_select_minor;
        send_recv(SEND, TASK_FS, &msg);
        tty->tty_select_ops &= ~ops;
    }

    return 0;
}

static int do_select(dev_t minor, int ops, endpoint_t endpoint)
{
    TTY* tty = minor2tty(minor);

    if (tty == NULL) {
        return -ENXIO;
    }

    int watch = ops & CDEV_NOTIFY;
    ops &= (CDEV_SEL_RD | CDEV_SEL_WR | CDEV_SEL_EXC);

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

/*****************************************************************************
 *                                tty_do_kern_log
 *****************************************************************************/
/**
 * Invoked when task TTY receives KERN_LOG message.
 *
 *****************************************************************************/
static void tty_do_kern_log()
{
    static int prev_next = 0;
    static char kernel_log_copy[KERN_LOG_SIZE];
    struct kern_log* klog = _sysinfo->kern_log;
    int next = klog->next;
    TTY* tty;

    size_t bytes = ((next + KERN_LOG_SIZE) - prev_next) % KERN_LOG_SIZE, copy;
    if (bytes > 0) {
        copy = min(bytes, KERN_LOG_SIZE - prev_next);
        memcpy(kernel_log_copy, &klog->buf[prev_next], copy);
        if (bytes > copy)
            memcpy(&kernel_log_copy[copy], klog->buf, bytes - copy);

        tty = minor2tty(cons_minor);
        /* tell the tty: */
        tty->tty_outcaller = TASK_TTY;     /* who wants to output the chars */
        tty->tty_outbuf = kernel_log_copy; /* where are the chars */
        tty->tty_outleft = bytes;          /* how many chars are requested */
        tty->tty_outcnt = 0;

        handle_events(tty);
    }

    prev_next = next;
}

/*****************************************************************************
 *                                erase
 *****************************************************************************/
/**
 * Erase last character.
 *
 *****************************************************************************/
static void erase(TTY* tty)
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

/*****************************************************************************
 *                                tty_echo
 *****************************************************************************/
/**
 * Echo the character is echoing is on.
 *
 *****************************************************************************/
static void tty_echo(TTY* tty, char c)
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

/*****************************************************************************
 *                                tty_sigproc
 *****************************************************************************/
/**
 * Send a signal to control proc.
 *
 *****************************************************************************/
static void tty_sigproc(TTY* tty, int signo)
{
    endpoint_t ep;
    if (get_procep(tty->tty_pgrp, &ep) != 0) return;

    int retval;
    if ((retval = kernel_kill(ep, signo)) != 0)
        panic("unable to send signal(%d)", retval);
}

/*****************************************************************************
 *                                tty_icancel
 *****************************************************************************/
/**
 * Cancel all pending inputs.
 *
 *****************************************************************************/
static void tty_icancel(TTY* tty)
{
    tty->tty_trans_cnt = tty->tty_eotcnt = 0;
    tty->ibuf_tail = tty->ibuf_head;
}
