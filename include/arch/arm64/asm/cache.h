#ifndef _ARCH_CACHE_H_
#define _ARCH_CACHE_H_

extern void dcache_clean_inval_poc(unsigned long start, unsigned long end);
extern void dcache_inval_poc(unsigned long start, unsigned long end);

#endif
