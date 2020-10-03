#define _GNU_SOURCE
#include <dlfcn.h>

static char dlfcn_error[] = "Service unavailable";

void* dlopen(const char* name, int mode)
    __attribute__((weak, alias("__dlopen")));
int dlclose(void* fd) __attribute__((weak, alias("__dlclose")));
void* dlsym(void* handle, const char* name)
    __attribute__((weak, alias("__dlsym")));
char* dlerror(void) __attribute__((weak, alias("__dlerror")));

void* dlvsym(void* handle, const char* name, const char* version)
    __attribute__((weak, alias("__dlvsym")));
int dladdr(const void* addr, Dl_info* dli)
    __attribute__((weak, alias("__dladdr")));
int dlinfo(void* handle, int req, void* v)
    __attribute__((weak, alias("__dlinfo")));

void* __dlopen(const char* name, int mode) { return NULL; }

int __dlclose(void* fd) { return -1; }

void* __dlsym(void* handle, const char* name) { return NULL; }

void* __dlvsym(void* handle, const char* name, const char* version)
{

    return NULL;
}

char* __dlerror(void) { return dlfcn_error; }

int __dladdr(const void* addr, Dl_info* dli) { return 0; }

int __dlinfo(void* handle, int req, void* v) { return -1; }
