/* see <sys/ucontext.h> */

#define UC_FLAGS 0
#define UC_LINK  8

#define GREGS_BASE 16

#define R8      (GREGS_BASE + 0 * 8)
#define R9      (GREGS_BASE + 1 * 8)
#define R10     (GREGS_BASE + 2 * 8)
#define R11     (GREGS_BASE + 3 * 8)
#define R12     (GREGS_BASE + 4 * 8)
#define R13     (GREGS_BASE + 5 * 8)
#define R14     (GREGS_BASE + 6 * 8)
#define R15     (GREGS_BASE + 7 * 8)
#define RDI     (GREGS_BASE + 8 * 8)
#define RSI     (GREGS_BASE + 9 * 8)
#define RBP     (GREGS_BASE + 10 * 8)
#define RBX     (GREGS_BASE + 11 * 8)
#define RDX     (GREGS_BASE + 12 * 8)
#define RAX     (GREGS_BASE + 13 * 8)
#define RCX     (GREGS_BASE + 14 * 8)
#define RSP     (GREGS_BASE + 15 * 8)
#define RIP     (GREGS_BASE + 16 * 8)
#define EFL     (GREGS_BASE + 17 * 8)
#define CSGSFS  (GREGS_BASE + 18 * 8)
#define ERR     (GREGS_BASE + 19 * 8)
#define TRAPNO  (GREGS_BASE + 20 * 8)
#define OLDMASK (GREGS_BASE + 21 * 8)
#define CR2     (GREGS_BASE + 22 * 8)
