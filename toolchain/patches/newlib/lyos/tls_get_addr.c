#include <stddef.h>

void* __tls_get_addr(void) __attribute__((weak, alias("__libc_tls_get_addr")));

void* ___tls_get_addr(void) __attribute__((weak, alias("__libc_tls_get_addr")));

void* __libc_tls_get_addr(void) { return NULL; }
