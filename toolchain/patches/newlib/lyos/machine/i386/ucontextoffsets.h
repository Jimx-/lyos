/* see <sys/ucontext.h> */

#define UC_FLAGS 0
#define UC_LINK 4

#define GREGS_BASE 8
#define DI GREGS_BASE + 4 * 4
#define SI GREGS_BASE + 5 * 4
#define BP GREGS_BASE + 6 * 4
#define AX GREGS_BASE + 11 * 4
#define BX GREGS_BASE + 8 * 4
#define CX GREGS_BASE + 10 * 4
#define DX GREGS_BASE + 9 * 4
#define PC GREGS_BASE + 14 * 4
#define SP GREGS_BASE + 7 * 4
