#include <assert.h>
#include <lyos/sysutils.h>

void __assert_func(const char* file, int line, const char* func,
                   const char* failedexpr)
{
    panic("assertion \"%s\" failed: file \"%s\", line %d%s%s\n", failedexpr,
          file, line, func ? ", function: " : "", func ? func : "");
}
