#ifndef _ARCH_SCONST_H_
#define _ARCH_SCONST_H_

#include <lyos/config.h>

#ifdef CONFIG_64BIT
#define REG_SIZE    8
#else
#define REG_SIZE    4
#endif

/* register offsets into stackframe */
#define P_STACKBASE 0
#define SEPCREG     P_STACKBASE
#define RAREG       SEPCREG + REG_SIZE
#define SPREG       RAREG + REG_SIZE
#define KERNELSPREG SPREG + REG_SIZE
#define GPREG       KERNELSPREG + REG_SIZE
#define TPREG       GPREG + REG_SIZE
#define T0REG       TPREG + REG_SIZE
#define T1REG       T0REG + REG_SIZE
#define T2REG       T1REG + REG_SIZE
#define S0REG       T2REG + REG_SIZE
#define S1REG       S0REG + REG_SIZE
#define A0REG       S1REG + REG_SIZE
#define A1REG       A0REG + REG_SIZE
#define A2REG       A1REG + REG_SIZE
#define A3REG       A2REG + REG_SIZE
#define A4REG       A3REG + REG_SIZE
#define A5REG       A4REG + REG_SIZE
#define A6REG       A5REG + REG_SIZE
#define A7REG       A6REG + REG_SIZE
#define S2REG       A7REG + REG_SIZE
#define S3REG       S2REG + REG_SIZE
#define S4REG       S3REG + REG_SIZE
#define S5REG       S4REG + REG_SIZE
#define S6REG       S5REG + REG_SIZE
#define S7REG       S6REG + REG_SIZE
#define S8REG       S7REG + REG_SIZE
#define S9REG       S8REG + REG_SIZE
#define S10REG      S9REG + REG_SIZE
#define S11REG      S10REG + REG_SIZE
#define T3REG       S11REG + REG_SIZE
#define T4REG       T3REG + REG_SIZE
#define T5REG       T4REG + REG_SIZE
#define T6REG       T5REG + REG_SIZE
#define SSTATUSREG  T6REG + REG_SIZE
#define SBADADDRREG SSTATUSREG + REG_SIZE
#define SCAUSEREG   SBADADDRREG + REG_SIZE

#define P_ORIGA0    SCAUSEREG + REG_SIZE
#define P_CPU       P_ORIGA0 + REG_SIZE

#define P_TBR       P_CPU + REG_SIZE

#define F0REG       P_TBR + REG_SIZE
#define F1REG       F0REG + 8
#define F2REG       F1REG + 8
#define F3REG       F2REG + 8
#define F4REG       F3REG + 8
#define F5REG       F4REG + 8
#define F6REG       F5REG + 8
#define F7REG       F6REG + 8
#define F8REG       F7REG + 8
#define F9REG       F8REG + 8
#define F10REG      F9REG + 8
#define F11REG      F10REG + 8
#define F12REG      F11REG + 8
#define F13REG      F12REG + 8
#define F14REG      F13REG + 8
#define F15REG      F14REG + 8
#define F16REG      F15REG + 8
#define F17REG      F16REG + 8
#define F18REG      F17REG + 8
#define F19REG      F18REG + 8
#define F20REG      F19REG + 8
#define F21REG      F20REG + 8
#define F22REG      F21REG + 8
#define F23REG      F22REG + 8
#define F24REG      F23REG + 8
#define F25REG      F24REG + 8
#define F26REG      F25REG + 8
#define F27REG      F26REG + 8
#define F28REG      F27REG + 8
#define F29REG      F28REG + 8
#define F30REG      F29REG + 8
#define F31REG      F30REG + 8
#define FCSRREG     F31REG + 8

#endif // _ARCH_SCONST_H_
