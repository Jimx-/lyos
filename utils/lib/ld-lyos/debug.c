#include "ldso.h"
#include "debug.h"

#include <stdarg.h>

int debug = 0;

void debug_printf(const char* fmt, ...)
{
    if (debug) {
        va_list ap;

        va_start(ap, fmt);
        xvprintf(fmt, ap);
        va_end(ap);
    }
}
