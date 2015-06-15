#ifndef	_ARCH_DIV64_H_
#define	_ARCH_DIV64_H_

#define do_div(n,base)			\
({									\
	unsigned long __low, __low2, __high, __rem;			\
	__low  = (n) & 0xffffffff;					\
	__high = (n) >> 32;						\
	if (__high) {							\
		__rem   = __high % (unsigned long)base;			\
		__high  = __high / (unsigned long)base;			\
		__low2  = __low >> 16;					\
		__low2 += __rem << 16;					\
		__rem   = __low2 % (unsigned long)base;			\
		__low2  = __low2 / (unsigned long)base;			\
		__low   = __low & 0xffff;				\
		__low  += __rem << 16;					\
		__rem   = __low  % (unsigned long)base;			\
		__low   = __low  / (unsigned long)base;			\
		n = __low  + (__low2 << 16) + (__high << 32);		\
	} else {							\
		__rem = __low % (unsigned long)base;			\
		n = (__low / (unsigned long)base);			\
	}								\
	__rem;								\
})

#endif
