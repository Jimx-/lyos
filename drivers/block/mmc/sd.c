#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <lyos/list.h>
#include <errno.h>
#include <asm/byteorder.h>

#include "mmc.h"
#include "mmchost.h"
#include "sd.h"

static const unsigned int tran_exp[] = {10000, 100000, 1000000, 10000000,
                                        0,     0,      0,       0};

static const unsigned char tran_mant[] = {
    0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80,
};

#define UNSTUFF_BITS(resp, start, size)                         \
    ({                                                          \
        const int __size = size;                                \
        const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1; \
        const int __off = 3 - ((start) / 32);                   \
        const int __shft = (start)&31;                          \
        u32 __res;                                              \
                                                                \
        __res = resp[__off] >> __shft;                          \
        if (__size + __shft > 32)                               \
            __res |= resp[__off - 1] << ((32 - __shft) % 32);   \
        __res& __mask;                                          \
    })

static int mmc_decode_csd(struct mmc_card* card)
{
    struct mmc_csd* csd = &card->csd;
    int csd_struct, e, m;
    u32* raw_csd = card->raw_csd;

    csd_struct = UNSTUFF_BITS(raw_csd, 126, 2);

    switch (csd_struct) {
    case 0:
        csd->cmdclass = UNSTUFF_BITS(raw_csd, 84, 12);

        m = UNSTUFF_BITS(raw_csd, 99, 4);
        e = UNSTUFF_BITS(raw_csd, 96, 3);
        csd->max_dtr = tran_exp[e] * tran_mant[m];

        e = UNSTUFF_BITS(raw_csd, 47, 3);
        m = UNSTUFF_BITS(raw_csd, 62, 12);
        csd->capacity = (1 + m) << (e + 2);

        csd->read_blkbits = UNSTUFF_BITS(raw_csd, 80, 4);
        csd->write_blkbits = UNSTUFF_BITS(raw_csd, 22, 4);
        break;

    case 1:
        card->state |= MMC_STATE_BLOCKADDR;

        csd->cmdclass = UNSTUFF_BITS(raw_csd, 84, 12);

        m = UNSTUFF_BITS(raw_csd, 99, 4);
        e = UNSTUFF_BITS(raw_csd, 96, 3);
        csd->max_dtr = tran_exp[e] * tran_mant[m];

        m = UNSTUFF_BITS(raw_csd, 48, 22);
        csd->capacity = (1 + m) << 10;

        csd->read_blkbits = 9;
        csd->write_blkbits = 9;
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static int mmc_decode_scr(struct mmc_card* card)
{
    struct mmc_scr* scr = &card->scr;
    unsigned int scr_struct;
    u32 raw_scr[4];

    raw_scr[3] = card->raw_csr[1];
    raw_scr[2] = card->raw_csr[0];

    scr_struct = UNSTUFF_BITS(raw_scr, 60, 4);
    if (scr_struct) return -EINVAL;

    scr->bus_widths = UNSTUFF_BITS(raw_scr, 48, 4);

    return 0;
}

static int mmc_app_cmd(struct mmc_host* host, struct mmc_card* card)
{
    struct mmc_command cmd = {};
    int err;

    cmd.opcode = MMC_APP_CMD;

    if (card) {
        cmd.arg = card->rca << 16;
        cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
    } else {
        cmd.arg = 0;
        cmd.flags = MMC_RSP_R1 | MMC_CMD_BCR;
    }

    err = mmc_wait_for_cmd(host, &cmd);
    if (err) return err;

    if (!(cmd.resp[0] & R1_APP_CMD)) return -EOPNOTSUPP;

    return 0;
}

static int mmc_wait_for_app_cmd(struct mmc_host* host, struct mmc_card* card,
                                struct mmc_command* cmd)
{
    struct mmc_request mrq = {};
    int err;

    err = mmc_app_cmd(host, card);
    if (err) return err;

    memset(cmd->resp, 0, sizeof(cmd->resp));
    mrq.cmd = cmd;
    cmd->data = NULL;

    mmc_wait_for_req(host, &mrq);

    err = cmd->error;
    return err;
}

int mmc_send_app_op_cond(struct mmc_host* host, u32 ocr, u32* rocr)
{
    struct mmc_command cmd = {};
    int i, err;

    cmd.opcode = SD_APP_OP_COND;
    cmd.arg = ocr;
    cmd.flags = MMC_RSP_R3 | MMC_CMD_BCR;

    for (i = 100; i; i--) {
        err = mmc_wait_for_app_cmd(host, NULL, &cmd);
        if (err) break;

        if (ocr == 0) break;

        if (cmd.resp[0] & MMC_CARD_BUSY) break;

        err = -ETIMEDOUT;

        usleep(10);
    }

    if (rocr) *rocr = cmd.resp[0];

    return err;
}

int mmc_app_set_bus_width(struct mmc_card* card, int width)
{
    struct mmc_command cmd = {};

    cmd.opcode = SD_APP_SET_BUS_WIDTH;
    cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

    switch (width) {
    case MMC_BUS_WIDTH_1:
        cmd.arg = SD_BUS_WIDTH_1;
        break;
    case MMC_BUS_WIDTH_4:
        cmd.arg = SD_BUS_WIDTH_4;
        break;
    default:
        return -EINVAL;
    }

    return mmc_wait_for_app_cmd(card->host, card, &cmd);
}

int mmc_send_if_cond(struct mmc_host* host, u32 ocr)
{
    struct mmc_command cmd = {};
    u8 test_pattern = 0xAA;
    u8 result_pattern;
    int err;

    cmd.opcode = SD_SEND_IF_COND;
    cmd.arg = ((ocr & 0xFF8000) != 0) << 8 | test_pattern;
    cmd.flags = MMC_RSP_R7 | MMC_CMD_BCR;

    err = mmc_wait_for_cmd(host, &cmd);
    if (err) return err;

    result_pattern = cmd.resp[0] & 0xFF;
    if (result_pattern != test_pattern) return -EIO;

    return 0;
}

int mmc_sd_get_cid(struct mmc_host* host, u32 ocr, u32* cid, u32* rocr)
{
    int err;

    mmc_go_idle(host);

    err = mmc_send_if_cond(host, ocr);
    if (!err) ocr |= SD_OCR_CCS;

    err = mmc_send_app_op_cond(host, ocr, rocr);
    if (err) return err;

    return mmc_send_cid(host, cid);
}

int mmc_send_relative_addr(struct mmc_host* host, unsigned int* rca)
{
    struct mmc_command cmd = {};
    int err;

    cmd.opcode = SD_SEND_RELATIVE_ADDR;
    cmd.arg = 0;
    cmd.flags = MMC_RSP_R6 | MMC_CMD_BCR;

    err = mmc_wait_for_cmd(host, &cmd);
    if (err) return err;

    *rca = cmd.resp[0] >> 16;

    return 0;
}

int mmc_app_send_scr(struct mmc_card* card)
{
    int err;
    struct mmc_request mrq = {};
    struct mmc_command cmd = {};
    struct mmc_data data = {};
    __be32* scr;

    err = mmc_app_cmd(card->host, card);
    if (err) return err;

    scr = malloc(sizeof(card->raw_csr));
    if (!scr) return -ENOMEM;

    mrq.cmd = &cmd;
    mrq.data = &data;

    cmd.opcode = SD_APP_SEND_SCR;
    cmd.arg = 0;
    cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

    data.blksz = 8;
    data.blocks = 1;
    data.flags = MMC_DATA_READ;
    data.data = scr;
    data.len = 8;

    mmc_wait_for_req(card->host, &mrq);

    card->raw_csr[0] = be32_to_cpu(scr[0]);
    card->raw_csr[1] = be32_to_cpu(scr[1]);

    free(scr);

    if (cmd.error) return cmd.error;
    if (data.error) return data.error;

    return 0;
}

static int mmc_sd_init_card(struct mmc_host* host, u32 ocr)
{
    struct mmc_card* card;
    u32 cid[4];
    u32 rocr;
    int err;

    err = mmc_sd_get_cid(host, ocr, cid, &rocr);
    if (err) return err;

    card = malloc(sizeof(*card));
    if (!card) return -ENOMEM;

    memset(card, 0, sizeof(*card));
    card->host = host;
    card->ocr = ocr;
    card->type = MMC_TYPE_SD;
    memcpy(card->raw_cid, cid, sizeof(card->raw_cid));

    err = mmc_send_relative_addr(host, &card->rca);
    if (err) goto free_card;

    err = mmc_send_csd(card, card->raw_csd);
    if (err) goto free_card;

    err = mmc_decode_csd(card);
    if (err) goto free_card;

    err = mmc_select_card(card);
    if (err) goto free_card;

    err = mmc_app_send_scr(card);
    if (err) goto free_card;

    err = mmc_decode_scr(card);
    if (err) goto free_card;

    mmc_set_clock(host, card->csd.max_dtr);

    if (card->scr.bus_widths & SD_SCR_BUS_WIDTH_4) {
        err = mmc_app_set_bus_width(card, MMC_BUS_WIDTH_4);
        if (err) goto free_card;

        mmc_set_bus_width(host, MMC_BUS_WIDTH_4);
    }

    host->card = card;
    return 0;

free_card:
    free(card);
    return err;
}

int mmc_attach_sd(struct mmc_host* host)
{
    u32 ocr;
    int err;

    err = mmc_send_app_op_cond(host, 0, &ocr);
    if (err) return err;

    ocr &= ~0x7FFF;

    ocr &= host->ocr_avail;
    if (!ocr) return -EINVAL;

    ocr &= 3UL << (30 - __builtin_clz(ocr));

    err = mmc_sd_init_card(host, ocr);
    if (err) return err;

    err = mmc_add_card(host->card);
    if (err) goto remove_card;

    return 0;

remove_card:
    mmc_remove_card(host->card);
    host->card = NULL;

    return err;
}
