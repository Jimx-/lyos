#ifndef _BITS_DIRENT_H_
#define _BITS_DIRENT_H_

struct dirent   {
    long d_ino; /* inode number */
    off_t d_off; /* offset to this dirent */
    unsigned short d_reclen; /* length of this d_name */
    unsigned char d_type; /* the type of d_name */
    char d_name [256]; /* file name (null-terminated) */
};

typedef struct DIR {
    int fd;
    void * buf;
    size_t size;
    size_t len;
    size_t loc;
} DIR;

enum
{
  DT_UNKNOWN = 0,
# define DT_UNKNOWN     DT_UNKNOWN
  DT_FIFO = 1,
# define DT_FIFO        DT_FIFO
  DT_CHR = 2,
# define DT_CHR         DT_CHR
  DT_DIR = 4,
# define DT_DIR         DT_DIR
  DT_BLK = 6,
# define DT_BLK         DT_BLK
  DT_REG = 8,
# define DT_REG         DT_REG
  DT_LNK = 10,
# define DT_LNK         DT_LNK
  DT_SOCK = 12,
# define DT_SOCK        DT_SOCK
  DT_WHT = 14
# define DT_WHT         DT_WHT
};

#endif
