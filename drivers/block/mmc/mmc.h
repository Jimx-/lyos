#ifndef _MMC_MMC_H_
#define _MMC_MMC_H_

/* Standard MMC commands (4.1)           type  argument     response */
/* class 1 */
#define MMC_GO_IDLE_STATE       0  /* bc                          */
#define MMC_SEND_OP_COND        1  /* bcr  [31:0] OCR         R3  */
#define MMC_ALL_SEND_CID        2  /* bcr                     R2  */
#define MMC_SET_RELATIVE_ADDR   3  /* ac   [31:16] RCA        R1  */
#define MMC_SET_DSR             4  /* bc   [31:16] RCA            */
#define MMC_SLEEP_AWAKE         5  /* ac   [31:16] RCA 15:flg R1b */
#define MMC_SWITCH              6  /* ac   [31:0] See below   R1b */
#define MMC_SELECT_CARD         7  /* ac   [31:16] RCA        R1  */
#define MMC_SEND_EXT_CSD        8  /* adtc                    R1  */
#define MMC_SEND_CSD            9  /* ac   [31:16] RCA        R2  */
#define MMC_SEND_CID            10 /* ac   [31:16] RCA        R2  */
#define MMC_READ_DAT_UNTIL_STOP 11 /* adtc [31:0] dadr        R1  */
#define MMC_STOP_TRANSMISSION   12 /* ac                      R1b */
#define MMC_SEND_STATUS         13 /* ac   [31:16] RCA        R1  */
#define MMC_BUS_TEST_R          14 /* adtc                    R1  */
#define MMC_GO_INACTIVE_STATE   15 /* ac   [31:16] RCA            */
#define MMC_BUS_TEST_W          19 /* adtc                    R1  */
#define MMC_SPI_READ_OCR        58 /* spi                  spi_R3 */
#define MMC_SPI_CRC_ON_OFF      59 /* spi  [0:0] flag      spi_R1 */

/* class 2 */
#define MMC_SET_BLOCKLEN            16 /* ac   [31:0] block len   R1  */
#define MMC_READ_SINGLE_BLOCK       17 /* adtc [31:0] data addr   R1  */
#define MMC_READ_MULTIPLE_BLOCK     18 /* adtc [31:0] data addr   R1  */
#define MMC_SEND_TUNING_BLOCK       19 /* adtc                    R1  */
#define MMC_SEND_TUNING_BLOCK_HS200 21 /* adtc R1  */

/* class 3 */
#define MMC_WRITE_DAT_UNTIL_STOP 20 /* adtc [31:0] data addr   R1  */

/* class 4 */
#define MMC_SET_BLOCK_COUNT      23 /* adtc [31:0] data addr   R1  */
#define MMC_WRITE_BLOCK          24 /* adtc [31:0] data addr   R1  */
#define MMC_WRITE_MULTIPLE_BLOCK 25 /* adtc                    R1  */
#define MMC_PROGRAM_CID          26 /* adtc                    R1  */
#define MMC_PROGRAM_CSD          27 /* adtc                    R1  */

/* class 6 */
#define MMC_SET_WRITE_PROT  28 /* ac   [31:0] data addr   R1b */
#define MMC_CLR_WRITE_PROT  29 /* ac   [31:0] data addr   R1b */
#define MMC_SEND_WRITE_PROT 30 /* adtc [31:0] wpdata addr R1  */

/* class 5 */
#define MMC_ERASE_GROUP_START 35 /* ac   [31:0] data addr   R1  */
#define MMC_ERASE_GROUP_END   36 /* ac   [31:0] data addr   R1  */
#define MMC_ERASE             38 /* ac                      R1b */

/* class 9 */
#define MMC_FAST_IO      39 /* ac   <Complex>          R4  */
#define MMC_GO_IRQ_STATE 40 /* bcr                     R5  */

/* class 7 */
#define MMC_LOCK_UNLOCK 42 /* adtc                    R1b */

/* class 8 */
#define MMC_APP_CMD 55 /* ac   [31:16] RCA        R1  */
#define MMC_GEN_CMD 56 /* adtc [0] RD/WR          R1  */

/* class 11 */
#define MMC_QUE_TASK_PARAMS    44 /* ac   [20:16] task id    R1  */
#define MMC_QUE_TASK_ADDR      45 /* ac   [31:0] data addr   R1  */
#define MMC_EXECUTE_READ_TASK  46 /* adtc [20:16] task id    R1  */
#define MMC_EXECUTE_WRITE_TASK 47 /* adtc [20:16] task id    R1  */
#define MMC_CMDQ_TASK_MGMT     48 /* ac   [20:16] task id    R1b */

#define R1_OUT_OF_RANGE       (1 << 31) /* er, c */
#define R1_ADDRESS_ERROR      (1 << 30) /* erx, c */
#define R1_BLOCK_LEN_ERROR    (1 << 29) /* er, c */
#define R1_ERASE_SEQ_ERROR    (1 << 28) /* er, c */
#define R1_ERASE_PARAM        (1 << 27) /* ex, c */
#define R1_WP_VIOLATION       (1 << 26) /* erx, c */
#define R1_CARD_IS_LOCKED     (1 << 25) /* sx, a */
#define R1_LOCK_UNLOCK_FAILED (1 << 24) /* erx, c */
#define R1_COM_CRC_ERROR      (1 << 23) /* er, b */
#define R1_ILLEGAL_COMMAND    (1 << 22) /* er, b */
#define R1_CARD_ECC_FAILED    (1 << 21) /* ex, c */
#define R1_CC_ERROR           (1 << 20) /* erx, c */
#define R1_ERROR              (1 << 19) /* erx, c */
#define R1_UNDERRUN           (1 << 18) /* ex, c */
#define R1_OVERRUN            (1 << 17) /* ex, c */
#define R1_CID_CSD_OVERWRITE  (1 << 16) /* erx, c, CID/CSD overwrite */
#define R1_WP_ERASE_SKIP      (1 << 15) /* sx, c */
#define R1_CARD_ECC_DISABLED  (1 << 14) /* sx, a */
#define R1_ERASE_RESET        (1 << 13) /* sr, c */
#define R1_STATUS(x)          (x & 0xFFF9A000)
#define R1_CURRENT_STATE(x)   ((x & 0x00001E00) >> 9) /* sx, b (4 bits) */
#define R1_READY_FOR_DATA     (1 << 8)                /* sx, a */
#define R1_SWITCH_ERROR       (1 << 7)                /* sx, c */
#define R1_EXCEPTION_EVENT    (1 << 6)                /* sr, a */
#define R1_APP_CMD            (1 << 5)                /* sr, c */

#define MMC_CARD_BUSY 0x80000000 /* Card Power up status bit */

/*
 * Card Command Classes (CCC)
 */
#define CCC_BASIC (1 << 0)        /* (0) Basic protocol functions */
                                  /* (CMD0,1,2,3,4,7,9,10,12,13,15) */
                                  /* (and for SPI, CMD58,59) */
#define CCC_STREAM_READ (1 << 1)  /* (1) Stream read commands */
                                  /* (CMD11) */
#define CCC_BLOCK_READ (1 << 2)   /* (2) Block read commands */
                                  /* (CMD16,17,18) */
#define CCC_STREAM_WRITE (1 << 3) /* (3) Stream write commands */
                                  /* (CMD20) */
#define CCC_BLOCK_WRITE (1 << 4)  /* (4) Block write commands */
                                  /* (CMD16,24,25,26,27) */
#define CCC_ERASE (1 << 5)        /* (5) Ability to erase blocks */
                                  /* (CMD32,33,34,35,36,37,38,39) */
#define CCC_WRITE_PROT (1 << 6)   /* (6) Able to write protect blocks */
                                  /* (CMD28,29,30) */
#define CCC_LOCK_CARD (1 << 7)    /* (7) Able to lock down card */
                                  /* (CMD16,CMD42) */
#define CCC_APP_SPEC (1 << 8)     /* (8) Application specific */
                                  /* (CMD55,56,57,ACMD*) */
#define CCC_IO_MODE (1 << 9)      /* (9) I/O mode */
                                  /* (CMD5,39,40,52,53) */
#define CCC_SWITCH (1 << 10)      /* (10) High speed switch */
                                  /* (CMD6,34,35,36,37,50) */
                                  /* (11) Reserved */
                                  /* (CMD?) */

struct mmc_request;
struct mmc_data;

struct mmc_command {
    u32 opcode;
    u32 arg;
    u32 resp[4];
    u32 flags;
#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_136     (1 << 1) /* 136 bit response */
#define MMC_RSP_CRC     (1 << 2) /* expect valid crc */
#define MMC_RSP_BUSY    (1 << 3) /* card may send busy */
#define MMC_RSP_OPCODE  (1 << 4) /* response contains opcode */

#define MMC_CMD_MASK (3 << 5) /* non-SPI command type */
#define MMC_CMD_AC   (0 << 5)
#define MMC_CMD_ADTC (1 << 5)
#define MMC_CMD_BC   (2 << 5)
#define MMC_CMD_BCR  (3 << 5)

#define MMC_RSP_SPI_S1   (1 << 7)  /* one status byte */
#define MMC_RSP_SPI_S2   (1 << 8)  /* second byte */
#define MMC_RSP_SPI_B4   (1 << 9)  /* four data bytes */
#define MMC_RSP_SPI_BUSY (1 << 10) /* card may send busy */

#define MMC_RSP_NONE (0)
#define MMC_RSP_R1   (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R1B \
    (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY)
#define MMC_RSP_R2 (MMC_RSP_PRESENT | MMC_RSP_136 | MMC_RSP_CRC)
#define MMC_RSP_R3 (MMC_RSP_PRESENT)
#define MMC_RSP_R4 (MMC_RSP_PRESENT)
#define MMC_RSP_R5 (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R6 (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R7 (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)

    int error;
    struct mmc_data* data;
    struct mmc_request* mrq;
};

struct mmc_data {
    unsigned int blksz;
    unsigned int blocks;
    unsigned int blk_addr;
    int error;

#define MMC_DATA_READ  (1 << 0)
#define MMC_DATA_WRITE (1 << 1)
    unsigned int flags;

    unsigned int bytes_xfered;
    struct mmc_request* mrq;

    void* data;
    unsigned int len;
};

struct mmc_request {
    struct mmc_command* cmd;
    struct mmc_data* data;
    unsigned int tid;
    int completed;

    void (*done)(struct mmc_request*);
};

#endif
