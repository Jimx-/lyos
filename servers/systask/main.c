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
#include <lyos/sysutils.h>
#include <lyos/portio.h>

#define MAX_HOSTNAME_LEN 256
static char hostname[MAX_HOSTNAME_LEN];
static struct utsname uname_buf;

#ifdef __i386__
#define RTC_YEAR     9 /* Clock register addresses in CMOS RAM	*/
#define RTC_MONTH    8
#define RTC_DAY      7
#define RTC_HOUR     4
#define RTC_MINUTE   2
#define RTC_SECOND   0
#define RTC_REG_A    0xA
#define RTC_A_UIP    0x80
#define RTC_REG_B    0x0B /* Status register B: RTC configuration	*/
#define RTC_B_DM_BCD 0x04

#define CMOS_STATUS                                    \
    0x0E /* Diagnostic status: (should be set by Power \
          * On Self-Test [POST])                       \
          * Bit  7 = RTC lost power                    \
          *	6 = Checksum (for addr 0x10-0x2d) bad      \
          *	5 = Config. Info. bad at POST              \
          *	4 = Mem. size error at POST                \
          *	3 = I/O board failed initialization        \
          *	2 = CMOS time invalid                      \
          *    1-0 =    reserved                       \
          */

static inline int read_register(int reg_addr);
#endif

static int get_rtc_time(struct time* t);
static int secs_of_years(int years);
static int secs_of_months(int months, int year);
static int do_gettimeofday(struct timeval* tv, struct timezone* tz);
static int prepare_uname_buf(struct utsname* buf);
static int do_getsethostname(MESSAGE* p);

#define BCD_TO_DEC(x) (((x) >> 4) & 0x0F) * 10 + ((x)&0x0F)

static void setup_boottime(void);

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

    setup_boottime();

    prepare_uname_buf(&uname_buf);

    while (1) {
        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;

        switch (msg.type) {
        case UNAME:
            msg.RETVAL = data_copy(src, msg.BUF, TASK_SYS, &uname_buf,
                                   sizeof(struct utsname));
            break;
        case GET_TIME_OF_DAY:
            msg.RETVAL = do_gettimeofday(&tv, NULL);
            data_copy(src, msg.BUF, TASK_SYS, &tv, sizeof(tv));
            break;
        case GETSETHOSTNAME:
            msg.RETVAL = do_getsethostname(&msg);
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
#ifdef __i386__
    int sec, n;

    /* do { */
    /*     sec = -1; */
    /*     n = 0; */
    /*     do { */
    /*         if (read_register(RTC_REG_A) & RTC_A_UIP) continue; */

    /*         t->second = read_register(RTC_SECOND); */
    /*         if (t->second != sec) { */
    /*             sec = t->second; */
    /*             n++; */
    /*         } */
    /*     } while (n < 2); */

    t->second = read_register(RTC_SECOND);
    t->minute = read_register(RTC_MINUTE);
    t->hour = read_register(RTC_HOUR);
    t->day = read_register(RTC_DAY);
    t->month = read_register(RTC_MONTH);
    t->year = read_register(RTC_YEAR);

    /* } while (read_register(RTC_SECOND) != t->second || */
    /*          read_register(RTC_MINUTE) != t->minute || */
    /*          read_register(RTC_HOUR) != t->hour || */
    /*          read_register(RTC_DAY) != t->day || */
    /*          read_register(RTC_MONTH) != t->month || */
    /*          read_register(RTC_YEAR) != t->year); */

    if (!(read_register(RTC_REG_B) & RTC_B_DM_BCD)) {
        t->year = BCD_TO_DEC(t->year);
        t->month = BCD_TO_DEC(t->month);
        t->day = BCD_TO_DEC(t->day);
        t->hour = BCD_TO_DEC(t->hour);
        t->minute = BCD_TO_DEC(t->minute);
        t->second = BCD_TO_DEC(t->second);
    }

    if (t->year < 80) t->year += 100;

    t->year += 1900;
#endif

    return 0;
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
    int v, retval;

    if ((retval = portio_outb(CLK_ELE, reg_addr)) != OK) {
        printl("read_register outb failed: %d\n", retval);
        return -1;
    }

    if ((retval = portio_inb(CLK_IO, &v)) != OK) {
        printl("read_register outb failed: %d\n", retval);
        return -1;
    }
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

static void setup_boottime(void)
{
    struct time t;
    time_t time;

    if (get_rtc_time(&t) != 0) return;

    time = secs_of_years(t.year - 1) + secs_of_months(t.month - 1, t.year) +
           (t.day - 1) * 86400 + t.hour * 3600 + t.minute * 60 + t.second;

    kernel_stime(time);
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
