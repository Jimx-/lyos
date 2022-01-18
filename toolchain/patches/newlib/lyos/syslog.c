#include <sys/syslog.h>

void openlog(const char* ident, int option, int facility) {}

void closelog(void) {}

void vsyslog(int priority, const char* format, va_list ap) {}

void syslog(int priority, const char* format, ...)
{
    va_list args;

    va_start(args, format);
    vsyslog(priority, format, args);
    va_end(args);
}
