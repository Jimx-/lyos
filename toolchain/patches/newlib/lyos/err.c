#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void verr(int eval, const char *fmt, va_list ap)
{
	int sverrno;

	sverrno = errno;
	(void)fprintf(stderr, "%s: ", getprogname());
	if (fmt != NULL) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	(void)fprintf(stderr, "%s\n", strerror(sverrno));
	exit(eval);
}

void verrx(int eval, const char *fmt, va_list ap)
{
	(void)fprintf(stderr, "%s: ", getprogname());
	if (fmt != NULL)
		(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");
	exit(eval);
}

void vwarn(const char *fmt, va_list ap)
{
	int sverrno;

	sverrno = errno;
	(void)fprintf(stderr, "%s: ", getprogname());
	if (fmt != NULL) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	(void)fprintf(stderr, "%s\n", strerror(sverrno));
}

void vwarnx(const char *fmt, va_list ap)
{
	(void)fprintf(stderr, "%s: ", getprogname());
	if (fmt != NULL)
		(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");
}

void err(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verr(eval, fmt, ap);
	va_end(ap);
}

void errx(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrx(eval, fmt, ap);
	va_end(ap);
}

void warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
}

void warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}
