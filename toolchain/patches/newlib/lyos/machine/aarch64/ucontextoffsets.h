#define REG_SIZE 8

#define UC_FLAGS 0
#define UC_LINK  (UC_FLAGS + REG_SIZE)

#define oX0     (UC_LINK + REG_SIZE)
#define oSP     (oX0 + 31 * REG_SIZE)
#define oPC     (oSP + REG_SIZE)
#define oPSTATE (oPC + REG_SIZE)
