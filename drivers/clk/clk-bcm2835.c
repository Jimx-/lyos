#include <lyos/const.h>
#include <stddef.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <asm/io.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#include "clk.h"

#define BCM2835_PLLA 0
#define BCM2835_PLLB 1
#define BCM2835_PLLC 2
#define BCM2835_PLLD 3
#define BCM2835_PLLH 4

#define BCM2835_PLLA_CORE  5
#define BCM2835_PLLA_PER   6
#define BCM2835_PLLB_ARM   7
#define BCM2835_PLLC_CORE0 8
#define BCM2835_PLLC_CORE1 9
#define BCM2835_PLLC_CORE2 10
#define BCM2835_PLLC_PER   11
#define BCM2835_PLLD_CORE  12
#define BCM2835_PLLD_PER   13
#define BCM2835_PLLH_RCAL  14
#define BCM2835_PLLH_AUX   15
#define BCM2835_PLLH_PIX   16

#define BCM2835_CLOCK_TIMER      17
#define BCM2835_CLOCK_OTP        18
#define BCM2835_CLOCK_UART       19
#define BCM2835_CLOCK_VPU        20
#define BCM2835_CLOCK_V3D        21
#define BCM2835_CLOCK_ISP        22
#define BCM2835_CLOCK_H264       23
#define BCM2835_CLOCK_VEC        24
#define BCM2835_CLOCK_HSM        25
#define BCM2835_CLOCK_SDRAM      26
#define BCM2835_CLOCK_TSENS      27
#define BCM2835_CLOCK_EMMC       28
#define BCM2835_CLOCK_PERI_IMAGE 29
#define BCM2835_CLOCK_PWM        30
#define BCM2835_CLOCK_PCM        31

#define BCM2835_PLLA_DSI0 32
#define BCM2835_PLLA_CCP2 33
#define BCM2835_PLLD_DSI0 34
#define BCM2835_PLLD_DSI1 35

#define BCM2835_CLOCK_AVEO  36
#define BCM2835_CLOCK_DFT   37
#define BCM2835_CLOCK_GP0   38
#define BCM2835_CLOCK_GP1   39
#define BCM2835_CLOCK_GP2   40
#define BCM2835_CLOCK_SLIM  41
#define BCM2835_CLOCK_SMI   42
#define BCM2835_CLOCK_TEC   43
#define BCM2835_CLOCK_DPI   44
#define BCM2835_CLOCK_CAM0  45
#define BCM2835_CLOCK_CAM1  46
#define BCM2835_CLOCK_DSI0E 47
#define BCM2835_CLOCK_DSI1E 48
#define BCM2835_CLOCK_DSI0P 49
#define BCM2835_CLOCK_DSI1P 50

#define BCM2711_CLOCK_EMMC2 51

#define CM_PASSWORD 0x5a000000

#define CM_GNRICCTL      0x000
#define CM_GNRICDIV      0x004
#define CM_DIV_FRAC_BITS 12
#define CM_DIV_FRAC_MASK GENMASK(CM_DIV_FRAC_BITS - 1, 0)

#define CM_VPUCTL   0x008
#define CM_VPUDIV   0x00c
#define CM_SYSCTL   0x010
#define CM_SYSDIV   0x014
#define CM_PERIACTL 0x018
#define CM_PERIADIV 0x01c
#define CM_PERIICTL 0x020
#define CM_PERIIDIV 0x024
#define CM_H264CTL  0x028
#define CM_H264DIV  0x02c
#define CM_ISPCTL   0x030
#define CM_ISPDIV   0x034
#define CM_V3DCTL   0x038
#define CM_V3DDIV   0x03c
#define CM_CAM0CTL  0x040
#define CM_CAM0DIV  0x044
#define CM_CAM1CTL  0x048
#define CM_CAM1DIV  0x04c
#define CM_CCP2CTL  0x050
#define CM_CCP2DIV  0x054
#define CM_DSI0ECTL 0x058
#define CM_DSI0EDIV 0x05c
#define CM_DSI0PCTL 0x060
#define CM_DSI0PDIV 0x064
#define CM_DPICTL   0x068
#define CM_DPIDIV   0x06c
#define CM_GP0CTL   0x070
#define CM_GP0DIV   0x074
#define CM_GP1CTL   0x078
#define CM_GP1DIV   0x07c
#define CM_GP2CTL   0x080
#define CM_GP2DIV   0x084
#define CM_HSMCTL   0x088
#define CM_HSMDIV   0x08c
#define CM_OTPCTL   0x090
#define CM_OTPDIV   0x094
#define CM_PCMCTL   0x098
#define CM_PCMDIV   0x09c
#define CM_PWMCTL   0x0a0
#define CM_PWMDIV   0x0a4
#define CM_SLIMCTL  0x0a8
#define CM_SLIMDIV  0x0ac
#define CM_SMICTL   0x0b0
#define CM_SMIDIV   0x0b4
/* no definition for 0x0b8  and 0x0bc */
#define CM_TCNTCTL         0x0c0
#define CM_TCNT_SRC1_SHIFT 12
#define CM_TCNTCNT         0x0c4
#define CM_TECCTL          0x0c8
#define CM_TECDIV          0x0cc
#define CM_TD0CTL          0x0d0
#define CM_TD0DIV          0x0d4
#define CM_TD1CTL          0x0d8
#define CM_TD1DIV          0x0dc
#define CM_TSENSCTL        0x0e0
#define CM_TSENSDIV        0x0e4
#define CM_TIMERCTL        0x0e8
#define CM_TIMERDIV        0x0ec
#define CM_UARTCTL         0x0f0
#define CM_UARTDIV         0x0f4
#define CM_VECCTL          0x0f8
#define CM_VECDIV          0x0fc
#define CM_PULSECTL        0x190
#define CM_PULSEDIV        0x194
#define CM_SDCCTL          0x1a8
#define CM_SDCDIV          0x1ac
#define CM_ARMCTL          0x1b0
#define CM_AVEOCTL         0x1b8
#define CM_AVEODIV         0x1bc
#define CM_EMMCCTL         0x1c0
#define CM_EMMCDIV         0x1c4
#define CM_EMMC2CTL        0x1d0
#define CM_EMMC2DIV        0x1d4

/* General bits for the CM_*CTL regs */
#define CM_ENABLE         BIT(4)
#define CM_KILL           BIT(5)
#define CM_GATE_BIT       6
#define CM_GATE           BIT(CM_GATE_BIT)
#define CM_BUSY           BIT(7)
#define CM_BUSYD          BIT(8)
#define CM_FRAC           BIT(9)
#define CM_SRC_SHIFT      0
#define CM_SRC_BITS       4
#define CM_SRC_MASK       0xf
#define CM_SRC_GND        0
#define CM_SRC_OSC        1
#define CM_SRC_TESTDEBUG0 2
#define CM_SRC_TESTDEBUG1 3
#define CM_SRC_PLLA_CORE  4
#define CM_SRC_PLLA_PER   4
#define CM_SRC_PLLC_CORE0 5
#define CM_SRC_PLLC_PER   5
#define CM_SRC_PLLC_CORE1 8
#define CM_SRC_PLLD_CORE  6
#define CM_SRC_PLLD_PER   6
#define CM_SRC_PLLH_AUX   7
#define CM_SRC_PLLC_CORE1 8
#define CM_SRC_PLLC_CORE2 9

#define CM_OSCCOUNT 0x100

#define CM_PLLA          0x104
#define CM_PLL_ANARST    BIT(8)
#define CM_PLLA_HOLDPER  BIT(7)
#define CM_PLLA_LOADPER  BIT(6)
#define CM_PLLA_HOLDCORE BIT(5)
#define CM_PLLA_LOADCORE BIT(4)
#define CM_PLLA_HOLDCCP2 BIT(3)
#define CM_PLLA_LOADCCP2 BIT(2)
#define CM_PLLA_HOLDDSI0 BIT(1)
#define CM_PLLA_LOADDSI0 BIT(0)

#define CM_PLLC           0x108
#define CM_PLLC_HOLDPER   BIT(7)
#define CM_PLLC_LOADPER   BIT(6)
#define CM_PLLC_HOLDCORE2 BIT(5)
#define CM_PLLC_LOADCORE2 BIT(4)
#define CM_PLLC_HOLDCORE1 BIT(3)
#define CM_PLLC_LOADCORE1 BIT(2)
#define CM_PLLC_HOLDCORE0 BIT(1)
#define CM_PLLC_LOADCORE0 BIT(0)

#define CM_PLLD          0x10c
#define CM_PLLD_HOLDPER  BIT(7)
#define CM_PLLD_LOADPER  BIT(6)
#define CM_PLLD_HOLDCORE BIT(5)
#define CM_PLLD_LOADCORE BIT(4)
#define CM_PLLD_HOLDDSI1 BIT(3)
#define CM_PLLD_LOADDSI1 BIT(2)
#define CM_PLLD_HOLDDSI0 BIT(1)
#define CM_PLLD_LOADDSI0 BIT(0)

#define CM_PLLH          0x110
#define CM_PLLH_LOADRCAL BIT(2)
#define CM_PLLH_LOADAUX  BIT(1)
#define CM_PLLH_LOADPIX  BIT(0)

#define CM_LOCK        0x114
#define CM_LOCK_FLOCKH BIT(12)
#define CM_LOCK_FLOCKD BIT(11)
#define CM_LOCK_FLOCKC BIT(10)
#define CM_LOCK_FLOCKB BIT(9)
#define CM_LOCK_FLOCKA BIT(8)

#define CM_EVENT    0x118
#define CM_DSI1ECTL 0x158
#define CM_DSI1EDIV 0x15c
#define CM_DSI1PCTL 0x160
#define CM_DSI1PDIV 0x164
#define CM_DFTCTL   0x168
#define CM_DFTDIV   0x16c

#define CM_PLLB         0x170
#define CM_PLLB_HOLDARM BIT(1)
#define CM_PLLB_LOADARM BIT(0)

#define A2W_PLLA_CTRL             0x1100
#define A2W_PLLC_CTRL             0x1120
#define A2W_PLLD_CTRL             0x1140
#define A2W_PLLH_CTRL             0x1160
#define A2W_PLLB_CTRL             0x11e0
#define A2W_PLL_CTRL_PRST_DISABLE BIT(17)
#define A2W_PLL_CTRL_PWRDN        BIT(16)
#define A2W_PLL_CTRL_PDIV_MASK    0x000007000
#define A2W_PLL_CTRL_PDIV_SHIFT   12
#define A2W_PLL_CTRL_NDIV_MASK    0x0000003ff
#define A2W_PLL_CTRL_NDIV_SHIFT   0

#define A2W_PLLA_ANA0 0x1010
#define A2W_PLLC_ANA0 0x1030
#define A2W_PLLD_ANA0 0x1050
#define A2W_PLLH_ANA0 0x1070
#define A2W_PLLB_ANA0 0x10f0

struct bcm2835_cprman {
    void* regs;

    struct clk_hw_onecell_data onecell;
};

struct bcm2835_clock_data {
    const char* name;

    u32 ctl_reg;
    u32 div_reg;

    u32 int_bits;
    u32 frac_bits;

    u32 tcnt_mux;
};

struct bcm2835_clk_desc {
    struct clk_hw* (*clk_register)(struct bcm2835_cprman* cprman,
                                   const void* data);
    const void* data;
};

#define _REGISTER(f, ...)                      \
    {                                          \
        .clk_register = f, .data = __VA_ARGS__ \
    }
#define REGISTER_CLK(...)              \
    _REGISTER(&bcm2835_register_clock, \
              &(struct bcm2835_clock_data){__VA_ARGS__})

extern void* boot_params;

static const char* const bcm2835_compat[] = {"brcm,bcm2835-cprman", NULL};

static struct clk_hw* bcm2835_register_clock(struct bcm2835_cprman* cprman,
                                             const void* data);

static const struct bcm2835_clk_desc clk_desc_array[] = {
    /* Arasan EMMC clock */
    [BCM2835_CLOCK_EMMC] = REGISTER_CLK(.name = "emmc", .ctl_reg = CM_EMMCCTL,
                                        .div_reg = CM_EMMCDIV, .int_bits = 4,
                                        .frac_bits = 8, .tcnt_mux = 39),

};

static inline void cprman_write(struct bcm2835_cprman* cprman, u32 reg, u32 val)
{
    writel(cprman->regs + reg, CM_PASSWORD | val);
}

static inline u32 cprman_read(struct bcm2835_cprman* cprman, u32 reg)
{
    return readl(cprman->regs + reg);
}

static struct clk_hw* bcm2835_register_clock(struct bcm2835_cprman* cprman,
                                             const void* data)
{
    const struct bcm2835_clock_data* clock_data = data;

    return NULL;
}

static int fdt_scan_bcm2835_clk(void* blob, unsigned long offset,
                                const char* name, int depth, void* arg)
{
    struct bcm2835_cprman* cprman;
    const size_t asize = sizeof(clk_desc_array) / sizeof(clk_desc_array[0]);
    const struct bcm2835_clk_desc* desc;
    phys_bytes base, size;
    int i, ret;

    if (!of_flat_dt_match(blob, offset, bcm2835_compat)) return 0;

    cprman =
        malloc(sizeof(struct bcm2835_cprman) + asize * sizeof(struct clk_hw*));
    if (!cprman) return 1;

    memset(cprman, 0,
           sizeof(struct bcm2835_cprman) + asize * sizeof(struct clk_hw*));

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) goto out_free;

    cprman->regs = mm_map_phys(SELF, base, size, MMP_IO);
    if (cprman->regs == MAP_FAILED) goto out_free;

    cprman->onecell.num = asize;
    for (i = 0; i < asize; i++) {
        desc = &clk_desc_array[i];
        if (desc->clk_register && desc->data) {
            cprman->onecell.hws[i] = desc->clk_register(cprman, desc->data);
        }
    }

    return 1;

out_free:
    free(cprman);
    return 0;
}

void bcm2835_clk_scan(void)
{
    of_scan_fdt(fdt_scan_bcm2835_clk, NULL, boot_params);
}
