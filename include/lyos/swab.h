#ifndef _SWAB_H_
#define _SWAB_H_

PRIVATE inline u32 __swab32(u32 x) {
    return ((u32)(                          \
    ((x & (u32)0x000000ffUL) << 24) |		\
	((x & (u32)0x0000ff00UL) <<  8) |		\
	((x & (u32)0x00ff0000UL) >>  8) |		\
	((x & (u32)0xff000000UL) >> 24)));
}

PRIVATE inline u32 __swab32p(const u32* p)
{
    return __swab32(*p);
}

#endif // _SWAB_H_
