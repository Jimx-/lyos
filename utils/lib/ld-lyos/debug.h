#ifndef _DEBUG_H_
#define _DEBUG_H_

extern int debug;

#define dbg(a) debug_printf a

void debug_printf(const char* fmt, ...);

#endif
