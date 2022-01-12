/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *        The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *        @(#)syslog.h        8.1 (Berkeley) 6/2/93
 */

#ifndef _SYS_SYSLOG_H
#define _SYS_SYSLOG_H 1

#include <sys/cdefs.h>
#include <stdarg.h>

#define LOG_PID    0x01
#define LOG_CONS   0x02
#define LOG_NDELAY 0x08
#define LOG_ODELAY 0x04
#define LOG_NOWAIT 0x10

#define LOG_KERN     (0 << 3)
#define LOG_USER     (1 << 3)
#define LOG_MAIL     (2 << 3)
#define LOG_DAEMON   (3 << 3)
#define LOG_AUTH     (4 << 3)
#define LOG_SYSLOG   (5 << 3)
#define LOG_LPR      (6 << 3)
#define LOG_NEWS     (7 << 3)
#define LOG_UUCP     (8 << 3)
#define LOG_CRON     (9 << 3)
#define LOG_AUTHPRIV (10 << 3)
#define LOG_FTP      (11 << 3)

#define LOG_LOCAL0 (16 << 3)
#define LOG_LOCAL1 (17 << 3)
#define LOG_LOCAL2 (18 << 3)
#define LOG_LOCAL3 (19 << 3)
#define LOG_LOCAL4 (20 << 3)
#define LOG_LOCAL5 (21 << 3)
#define LOG_LOCAL6 (22 << 3)
#define LOG_LOCAL7 (23 << 3)

#define LOG_PRIMASK       7
#define LOG_PRI(p)        ((p)&LOG_PRIMASK)
#define LOG_MAKEPRI(f, p) (((f) << 3) | (p))
#define LOG_MASK(p)       (1 << (p))
#define LOG_UPTO(p)       ((1 << ((p) + 1)) - 1)
#define LOG_NFACILITIES   24
#define LOG_FACMASK       (0x7F << 3)
#define LOG_FAC(p)        (((p)&LOG_FACMASK) >> 3)

#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

__BEGIN_DECLS

void closelog(void);
void openlog(const char*, int, int);
int setlogmask(int);
void syslog(int, const char*, ...);
void vsyslog(int priority, const char* format, va_list ap);

__END_DECLS

#endif /* sys/syslog.h */
