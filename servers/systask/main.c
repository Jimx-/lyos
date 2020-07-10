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

#include "lyos/compile.h"
#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "lyos/config.h"
#include "stdio.h"
#include "unistd.h"
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/time.h"
#include "sys/utsname.h"
#include <sys/time.h>
#include <lyos/portio.h>

#define MAX_HOSTNAME_LEN 256
static char hostname[MAX_HOSTNAME_LEN];
static struct utsname uname_buf;

#ifdef __i386__
static inline int read_register(int reg_addr);
#endif

static int get_rtc_time(struct time* t);
static int secs_of_years(int years);
static int secs_of_months(int months, int year);
static int do_gettimeofday(struct timeval* tv, struct timezone* tz);
static int prepare_uname_buf(struct utsname* buf);
static int do_getsethostname(MESSAGE* p);

#define BCD_TO_DEC(x) ((x >> 4) * 10 + (x & 0x0f))

/*****************************************************************************
 *                                task_sys
 *****************************************************************************/
/**
 * <Ring 1> The main loop of TASK SYS.
 *
 *****************************************************************************/
int main()
{
    MESSAGE msg;
    struct timeval tv;

    prepare_uname_buf(&uname_buf);

    while (1) {
        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;

        switch (msg.type) {
        case UNAME:
            // printl(">>>>>>>> enter UNAME\n");
            msg.RETVAL = data_copy(src, msg.BUF, TASK_SYS, &uname_buf,
                                   sizeof(struct utsname));
            // printl("<<<<<<<< exit UNAME\n");
            break;
        case GET_TIME_OF_DAY:
            // printl(">>>>>>>> enter GET_TIME_OF_DAY\n");
            msg.RETVAL = do_gettimeofday(&tv, NULL);
            data_copy(src, msg.BUF, TASK_SYS, &tv, sizeof(tv));
            // printl("<<<<<<<< exit GET_TIME_OF_DAY\n");
            break;
        case GETSETHOSTNAME:
            // printl(">>>>>>>> enter GET_SET_HOSTNAME\n");
            msg.RETVAL = do_getsethostname(&msg);
            // printl("<<<<<<<< exit GET_SET_HOSTNAME\n");
            break;
        default:
            msg.RETVAL = ENOSYS;
            break;
        }

        msg.type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, src, &msg);
    }

    return 0;
}

/*****************************************************************************
 *                                get_rtc_time
 *****************************************************************************/
/**
 * Get RTC time from the CMOS
 *
 * @return Zero.
 *****************************************************************************/
static int get_rtc_time(struct time* t)
{
    int retval;
#ifdef __i386__
    t->year = read_register(YEAR);
    t->month = read_register(MONTH);
    t->day = read_register(DAY);
    t->hour = read_register(HOUR);
    t->minute = read_register(MINUTE);
    t->second = read_register(SECOND);

    if ((retval = read_register(CLK_STATUS) & 0x04) == 0) {
        /* Convert BCD to binary (default RTC mode) */
        t->year = BCD_TO_DEC(t->year);
        t->month = BCD_TO_DEC(t->month);
        t->day = BCD_TO_DEC(t->day);
        t->hour = BCD_TO_DEC(t->hour);
        t->minute = BCD_TO_DEC(t->minute);
        t->second = BCD_TO_DEC(t->second);
    }

    t->year += 2000;
#endif

    return retval;
}

#ifdef __i386__
/*****************************************************************************
 *                                read_register
 *****************************************************************************/
/**
 * Read register from CMOS.
 *
 * @param reg_addr
 *
 * @return
 *****************************************************************************/
static inline int read_register(int reg_addr)
{
    int v;
    portio_outb(CLK_ELE, reg_addr);
    portio_inb(CLK_IO, &v);
    return v;
}
#endif

static int secs_of_years(int years)
{
    int days = 0;
    while (years > 1969) {
        days += 365;
        if (years % 4 == 0) {
            if (years % 100 == 0) {
                if (years % 400 == 0) {
                    days++;
                }
            } else {
                days++;
            }
        }
        years--;
    }
    return days * 86400;
}

static int secs_of_months(int months, int year)
{
    int days = 0;
    switch (months) {
    case 11:
        days += 30;
    case 10:
        days += 31;
    case 9:
        days += 30;
    case 8:
        days += 31;
    case 7:
        days += 31;
    case 6:
        days += 30;
    case 5:
        days += 31;
    case 4:
        days += 30;
    case 3:
        days += 31;
    case 2:
        days += 28;
        if ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0))) {
            days++;
        }
    case 1:
        days += 31;
    default:
        break;
    }
    return days * 86400;
}

static int do_gettimeofday(struct timeval* tv, struct timezone* tz)
{
    struct time t;
    int ret;
    if ((ret = get_rtc_time(&t)) != 0) return ret;

    int time = secs_of_years(t.year - 1) + secs_of_months(t.month - 1, t.year) +
               (t.day - 1) * 86400 + t.hour * 3600 + t.minute * 60 + t.second;

    tv->tv_sec = time;
    tv->tv_usec = 0;
    return 0;
}

static int prepare_uname_buf(struct utsname* buf)
{
    memset(buf, 0, sizeof(struct utsname));
    memset(hostname, 0, sizeof(hostname));
    sprintf(hostname, "%s", CONFIG_DEFAULT_HOSTNAME);

    strcpy(buf->sysname, "Lyos");
    strcpy(buf->version, UTS_VERSION);
    strcpy(buf->machine, UTS_MACHINE);
    strcpy(buf->release, UTS_RELEASE);
    sprintf(buf->nodename, "%s", CONFIG_DEFAULT_HOSTNAME);

    return 0;
}

static int do_getsethostname(MESSAGE* p)
{
    int src = p->source;

    if (p->BUF_LEN < 0) return EINVAL;

    if (p->REQUEST == GS_GETHOSTNAME) {
        data_copy(src, p->BUF, TASK_SYS, hostname, p->BUF_LEN);
        if (strlen(hostname) > p->BUF_LEN) return ENAMETOOLONG;
    } else if (p->REQUEST == GS_SETHOSTNAME) {
        if (p->BUF_LEN > sizeof(hostname)) return EINVAL;
        memset(hostname, 0, sizeof(hostname));
        data_copy(TASK_SYS, hostname, src, p->BUF, p->BUF_LEN);
        hostname[p->BUF_LEN] = '\0';
        memcpy(uname_buf.nodename, hostname, _UTSNAME_NODENAME_LENGTH);
        uname_buf.nodename[_UTSNAME_NODENAME_LENGTH - 1] = '\0';
    }

    return 0;
}
