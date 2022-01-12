#include <sys/syslog.h>

void openlog(const char* ident, int option, int facility) {}

void closelog(void) {}

void vsyslog(int __pri, __const char* __fmt, va_list __ap) {}
