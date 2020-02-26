#ifndef _ARCH_SETUP_H
#define _ARCH_SETUP_H

/* The list ends with an ATAG_NONE node. */
#define ATAG_NONE 0x00000000
#define ATAG_CORE 0x54410001
#define ATAG_MEM 0x54410002
#define ATAG_VIDEOTEXT 0x54410003
#define ATAG_RAMDISK 0x54410004
#define ATAG_INITRD 0x54410005
#define ATAG_INITRD2 0x54420005
#define ATAG_SERIAL 0x54410006
#define ATAG_REVISION 0x54410007
#define ATAG_VIDEOLFB 0x54410008
#define ATAG_CMDLINE 0x54410009
#define ATAG_ACORN 0x41000101
#define ATAG_MEMCLK 0x41000402

struct tag_header {
    u32 size;
    u32 tag;
};

struct tag_core {
    u32 flags;
    u32 pagesize;
    u32 rootdev;
};

struct tag_mem32 {
    u32 size;
    u32 start;
};

struct tag_videotext {
    u8 x;
    u8 y;
    u16 video_page;
    u8 video_mode;
    u8 video_cols;
    u16 video_ega_bx;
    u8 video_lines;
    u8 video_isvga;
    u16 video_points;
};

struct tag_ramdisk {
    u32 flags;
    u32 size;
    u32 start;
};

struct tag_initrd {
    u32 start;
    u32 size;
};

struct tag_serialnr {
    u32 low;
    u32 high;
};

struct tag_revision {
    u32 rev;
};

struct tag_videolfb {
    u16 lfb_width;
    u16 lfb_height;
    u16 lfb_depth;
    u16 lfb_linelength;
    u32 lfb_base;
    u32 lfb_size;
    u8 red_size;
    u8 red_pos;
    u8 green_size;
    u8 green_pos;
    u8 blue_size;
    u8 blue_pos;
    u8 rsvd_size;
    u8 rsvd_pos;
};

struct tag_cmdline {
    char cmdline[1];
};

struct tag_acorn {
    u32 memc_control_reg;
    u32 vram_pages;
    u8 sounddefault;
    u8 adfsdrives;
};

struct tag_memclk {
    u32 fmemclk;
};

struct tag {
    struct tag_header hdr;
    union {
        struct tag_core core;
        struct tag_mem32 mem;
        struct tag_videotext videotext;
        struct tag_ramdisk ramdisk;
        struct tag_initrd initrd;
        struct tag_serialnr serialnr;
        struct tag_revision revision;
        struct tag_videolfb videolfb;
        struct tag_cmdline cmdline;

        /*
         * Acorn specific
         */
        struct tag_acorn acorn;

        /*
         * DC21285 specific
         */
        struct tag_memclk memclk;
    } u;
};

#endif
