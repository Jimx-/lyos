#ifndef _MMC_SD_H_
#define _MMC_SD_H_

/* SD commands                           type  argument     response */
/* class 0 */
/* This is basically the same command as for MMC with some quirks. */
#define SD_SEND_RELATIVE_ADDR 3  /* bcr                     R6  */
#define SD_SEND_IF_COND       8  /* bcr  [11:0] See below   R7  */
#define SD_SWITCH_VOLTAGE     11 /* ac                      R1  */

/* Application commands */
#define SD_APP_SET_BUS_WIDTH    6  /* ac   [1:0] bus width    R1  */
#define SD_APP_SD_STATUS        13 /* adtc                    R1  */
#define SD_APP_SEND_NUM_WR_BLKS 22 /* adtc                    R1  */
#define SD_APP_OP_COND          41 /* bcr  [31:0] OCR         R3  */
#define SD_APP_SEND_SCR         51 /* adtc                    R1  */

/* OCR bit definitions */
#define SD_OCR_S18R  (1 << 24)   /* 1.8V switching request */
#define SD_ROCR_S18A SD_OCR_S18R /* 1.8V switching accepted by card */
#define SD_OCR_XPC   (1 << 28)   /* SDXC power control */
#define SD_OCR_CCS   (1 << 30)   /* Card Capacity Status */

#define SD_BUS_WIDTH_1 0
#define SD_BUS_WIDTH_4 2

#endif
