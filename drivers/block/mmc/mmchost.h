#ifndef _MMC_MMCHOST_H_
#define _MMC_MMCHOST_H_

#include <lyos/list.h>

struct mmc_request;
struct mmc_command;
struct mmc_host;

struct mmc_csd {
    unsigned char structure;
    unsigned short cmdclass;
    unsigned int max_dtr;
    unsigned int read_blkbits;
    unsigned int write_blkbits;
    unsigned int capacity;
};

struct mmc_scr {
    unsigned char bus_widths;
#define SD_SCR_BUS_WIDTH_1 (1 << 0)
#define SD_SCR_BUS_WIDTH_4 (1 << 2)
};

struct mmc_card {
    struct mmc_host* host;
    int dev_id;
    u32 ocr;
    unsigned type;
#define MMC_TYPE_SD 1

    unsigned int rca;

    u32 raw_cid[4];
    u32 raw_csd[4];
    u32 raw_csr[2];
    struct mmc_csd csd;
    struct mmc_scr scr;
};

struct mmc_ios {
    unsigned int clock;
    unsigned char bus_width;
#define MMC_BUS_WIDTH_1 0
#define MMC_BUS_WIDTH_4 2
#define MMC_BUS_WIDTH_8 3
};

struct mmc_host_ops {
    void (*request)(struct mmc_host* host, struct mmc_request* req);
    void (*set_ios)(struct mmc_host* host, struct mmc_ios* ios);
    void (*hw_intr)(struct mmc_host* host);
};

struct mmc_host {
    struct list_head list;
    int index;
    int dev_id;

    unsigned int ocr_avail;
#define MMC_VDD_165_195 0x00000080 /* VDD voltage 1.65 - 1.95 */
#define MMC_VDD_20_21   0x00000100 /* VDD voltage 2.0 ~ 2.1 */
#define MMC_VDD_21_22   0x00000200 /* VDD voltage 2.1 ~ 2.2 */
#define MMC_VDD_22_23   0x00000400 /* VDD voltage 2.2 ~ 2.3 */
#define MMC_VDD_23_24   0x00000800 /* VDD voltage 2.3 ~ 2.4 */
#define MMC_VDD_24_25   0x00001000 /* VDD voltage 2.4 ~ 2.5 */
#define MMC_VDD_25_26   0x00002000 /* VDD voltage 2.5 ~ 2.6 */
#define MMC_VDD_26_27   0x00004000 /* VDD voltage 2.6 ~ 2.7 */
#define MMC_VDD_27_28   0x00008000 /* VDD voltage 2.7 ~ 2.8 */
#define MMC_VDD_28_29   0x00010000 /* VDD voltage 2.8 ~ 2.9 */
#define MMC_VDD_29_30   0x00020000 /* VDD voltage 2.9 ~ 3.0 */
#define MMC_VDD_30_31   0x00040000 /* VDD voltage 3.0 ~ 3.1 */
#define MMC_VDD_31_32   0x00080000 /* VDD voltage 3.1 ~ 3.2 */
#define MMC_VDD_32_33   0x00100000 /* VDD voltage 3.2 ~ 3.3 */
#define MMC_VDD_33_34   0x00200000 /* VDD voltage 3.3 ~ 3.4 */
#define MMC_VDD_34_35   0x00400000 /* VDD voltage 3.4 ~ 3.5 */
#define MMC_VDD_35_36   0x00800000 /* VDD voltage 3.5 ~ 3.6 */

    int irq;

    struct mmc_ios ios;

    const struct mmc_host_ops* ops;

    struct mmc_card* card;

    unsigned long private[];
};

static inline void* mmc_priv(struct mmc_host* host)
{
    return (void*)host->private;
}

static inline struct mmc_host* mmc_from_priv(void* priv)
{
    return list_entry(priv, struct mmc_host, private);
}

struct mmc_host* mmc_alloc_host(int extra);
int mmc_add_host(struct mmc_host* host);

int mmc_wait_for_cmd(struct mmc_host* host, struct mmc_command* cmd);
void mmc_wait_for_req(struct mmc_host* host, struct mmc_request* mrq);

void mmc_request_done(struct mmc_host* host, struct mmc_request* mrq);

int mmc_go_idle(struct mmc_host* host);
int mmc_send_cid(struct mmc_host* host, u32* cid);
int mmc_send_csd(struct mmc_card* card, u32* csd);

int mmc_select_card(struct mmc_card* card);
int mmc_deselect_card(struct mmc_host* host);

void mmc_set_clock(struct mmc_host* host, unsigned int hz);
void mmc_set_bus_width(struct mmc_host* host, unsigned int width);

#endif
