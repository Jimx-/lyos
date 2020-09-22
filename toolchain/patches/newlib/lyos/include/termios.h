#ifndef _TERMIOS_H
#define _TERMIOS_H

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define NCCS                                   \
    20 /* size of cc_c array, some extra space \
        * for extensions. */

typedef unsigned short tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

/* Primary terminal control structure. POSIX Table 7-1. */
struct termios {
    tcflag_t c_iflag; /* input modes */
    tcflag_t c_oflag; /* output modes */
    tcflag_t c_cflag; /* control modes */
    tcflag_t c_lflag; /* local modes */
    speed_t c_ispeed; /* input speed */
    speed_t c_ospeed; /* output speed */
    cc_t c_cc[NCCS];  /* control characters */
};

/* c_cc characters */
#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSWTC    7
#define VSTART   8
#define VSTOP    9
#define VSUSP    10
#define VEOL     11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE  14
#define VLNEXT   15
#define VEOL2    16

/* c_iflag bits */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IUCLC   0001000
#define IXON    0002000
#define IXANY   0004000
#define IXOFF   0010000
#define IMAXBEL 0020000

/* c_oflag bits */
#define OPOST  0000001
#define OLCUC  0000002
#define ONLCR  0000004
#define OCRNL  0000010
#define ONOCR  0000020
#define ONLRET 0000040
#define OFILL  0000100
#define OFDEL  0000200
#define NLDLY  0000400
#define NL0    0000000
#define NL1    0000400
#define CRDLY  0003000
#define CR0    0000000
#define CR1    0001000
#define CR2    0002000
#define CR3    0003000
#define TABDLY 0014000
#define TAB0   0000000
#define TAB1   0004000
#define TAB2   0010000
#define TAB3   0014000
#define XTABS  0014000
#define BSDLY  0020000
#define BS0    0000000
#define BS1    0020000
#define VTDLY  0040000
#define VT0    0000000
#define VT1    0040000
#define FFDLY  0040000
#define FF0    0000000
#define FF1    0040000

/* c_cflag bit meaning */
#define CBAUD   0000017
#define B0      0000000 /* hang up */
#define B50     0000001
#define B75     0000002
#define B110    0000003
#define B134    0000004
#define B150    0000005
#define B200    0000006
#define B300    0000007
#define B600    0000010
#define B1200   0000011
#define B1800   0000012
#define B2400   0000013
#define B4800   0000014
#define B9600   0000015
#define B19200  0000016
#define B38400  0000017
#define CSIZE   0000060
#define CS5     0000000
#define CS6     0000020
#define CS7     0000040
#define CS8     0000060
#define CSTOPB  0000100
#define CREAD   0000200
#define CPARENB 0000400
#define PARENB  CPARENB
#define CPARODD 0001000
#define PARODD  CPARODD
#define HUPCL   0002000
#define CLOCAL  0004000
#define CIBAUD  03600000     /* input baud rate (not used) */
#define CRTSCTS 020000000000 /* flow control */

/* c_lflag bits */
#define ISIG    0000001
#define ICANON  0000002
#define XCASE   0000004
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define ECHOCTL 0001000
#define ECHOPRT 0002000
#define ECHOKE  0004000
#define FLUSHO  0010000
#define PENDIN  0040000
#define IEXTEN  0100000

/* modem lines */
#define TIOCM_LE  0x001
#define TIOCM_DTR 0x002
#define TIOCM_RTS 0x004
#define TIOCM_ST  0x008
#define TIOCM_SR  0x010
#define TIOCM_CTS 0x020
#define TIOCM_CAR 0x040
#define TIOCM_RNG 0x080
#define TIOCM_DSR 0x100
#define TIOCM_CD  TIOCM_CAR
#define TIOCM_RI  TIOCM_RNG

/* tcflow() and TCXONC use these */
#define TCOOFF 0
#define TCOON  1
#define TCIOFF 2
#define TCION  3

/* tcflush() and TCFLSH use these */
#define TCIFLUSH  0
#define TCOFLUSH  1
#define TCIOFLUSH 2

/* tcsetattr uses these */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

#define TCTRL_DEF   (CREAD | CS8 | HUPCL)
#define TINPUT_DEF  (BRKINT | ICRNL | IXON | IXANY)
#define TOUTPUT_DEF (OPOST | ONLCR)
#define TLOCAL_DEF  (ISIG | IEXTEN | ICANON | ECHO | ECHOE)
#define TSPEED_DEF  B9600

#define _CTRL(x)     (x & 0x1f)
#define TEOF_DEF     _CTRL('d') /* ^D */
#define TEOL_DEF     0xFF
#define TERASE_DEF   _CTRL('h') /* ^H */
#define TINTR_DEF    _CTRL('c') /* ^C */
#define TKILL_DEF    _CTRL('u') /* ^U */
#define TMIN_DEF     1
#define TQUIT_DEF    _CTRL('\\') /* ^\ */
#define TSTART_DEF   _CTRL('q')  /* ^Q */
#define TSTOP_DEF    _CTRL('s')  /* ^S */
#define TSUSP_DEF    _CTRL('z')  /* ^Z */
#define TTIME_DEF    0
#define TREPRINT_DEF _CTRL('r') /* ^R */
#define TLNEXT_DEF   _CTRL('v') /* ^V */
#define TDISCARD_DEF _CTRL('o') /* ^O */

int tcgetattr(int fd, struct termios* tio);

#endif
