#ifndef _TAR_H_
#define _TAR_H_

struct posix_tar_header { /* byte offset */
    char name[100];       /*   0 */
    char mode[8];         /* 100 */
    char uid[8];          /* 108 */
    char gid[8];          /* 116 */
    char size[12];        /* 124 */
    char mtime[12];       /* 136 */
    char chksum[8];       /* 148 */
    char typeflag;        /* 156 */
    char linkname[100];   /* 157 */
    char magic[6];        /* 257 */
    char version[2];      /* 263 */
    char uname[32];       /* 265 */
    char gname[32];       /* 297 */
    char devmajor[8];     /* 329 */
    char devminor[8];     /* 337 */
    char prefix[155];     /* 345 */
                          /* 500 */
};

#define TAR_MAX_PATH 100

/* Values used in typeflag field.  */
#define REGTYPE  '0'  /* regular file */
#define AREGTYPE '\0' /* regular file */
#define LNKTYPE  '1'  /* link */
#define SYMTYPE  '2'  /* reserved */
#define CHRTYPE  '3'  /* character special */
#define BLKTYPE  '4'  /* block special */
#define DIRTYPE  '5'  /* directory */
#define FIFOTYPE '6'  /* FIFO special */
#define CONTTYPE '7'  /* reserved */

#endif
