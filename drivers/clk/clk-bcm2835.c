#include <lyos/const.h>
#include <stddef.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include <asm/io.h>
#include <lyos/list.h>
#include <asm/div64.h>

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

#define A2W_PLL_KA_SHIFT 7
#define A2W_PLL_KA_MASK  (0x7 << A2W_PLL_KA_SHIFT)
#define A2W_PLL_KI_SHIFT 19
#define A2W_PLL_KI_MASK  (0x7 << A2W_PLL_KI_SHIFT)
#define A2W_PLL_KP_SHIFT 15
#define A2W_PLL_KP_MASK  (0xf << A2W_PLL_KP_SHIFT)

#define A2W_XOSC_CTRL             0x1190
#define A2W_XOSC_CTRL_PLLB_ENABLE BIT(7)
#define A2W_XOSC_CTRL_PLLA_ENABLE BIT(6)
#define A2W_XOSC_CTRL_PLLD_ENABLE BIT(5)
#define A2W_XOSC_CTRL_DDR_ENABLE  BIT(4)
#define A2W_XOSC_CTRL_CPR1_ENABLE BIT(3)
#define A2W_XOSC_CTRL_USB_ENABLE  BIT(2)
#define A2W_XOSC_CTRL_HDMI_ENABLE BIT(1)
#define A2W_XOSC_CTRL_PLLC_ENABLE BIT(0)

#define A2W_PLLA_FRAC     0x1200
#define A2W_PLLC_FRAC     0x1220
#define A2W_PLLD_FRAC     0x1240
#define A2W_PLLH_FRAC     0x1260
#define A2W_PLLB_FRAC     0x12e0
#define A2W_PLL_FRAC_MASK ((1 << A2W_PLL_FRAC_BITS) - 1)
#define A2W_PLL_FRAC_BITS 20

#define A2W_PLL_CHANNEL_DISABLE BIT(8)
#define A2W_PLL_DIV_BITS        8
#define A2W_PLL_DIV_SHIFT       0

#define A2W_PLLC_CORE2 0x1320
#define A2W_PLLC_CORE1 0x1420
#define A2W_PLLC_PER   0x1520
#define A2W_PLLC_CORE0 0x1620

#define BCM2835_MAX_FB_RATE 1750000000u

static const char* const cprman_parent_names[] = {
    "xosc",      "dsi0_byte", "dsi0_ddr2", "dsi0_ddr",
    "dsi1_byte", "dsi1_ddr2", "dsi1_ddr",
};

struct bcm2835_cprman {
    void* regs;

    const char* parent_names[sizeof(cprman_parent_names) /
                             sizeof(cprman_parent_names[0])];

    struct clk_hw_onecell_data onecell;
};

struct bcm2835_pll_ana_bits {
    u32 mask0;
    u32 set0;
    u32 mask1;
    u32 set1;
    u32 mask3;
    u32 set3;
    u32 fb_prediv_mask;
};

static const struct bcm2835_pll_ana_bits bcm2835_ana_default = {
    .mask0 = 0,
    .set0 = 0,
    .mask1 = A2W_PLL_KI_MASK | A2W_PLL_KP_MASK,
    .set1 = (2 << A2W_PLL_KI_SHIFT) | (8 << A2W_PLL_KP_SHIFT),
    .mask3 = A2W_PLL_KA_MASK,
    .set3 = (2 << A2W_PLL_KA_SHIFT),
    .fb_prediv_mask = BIT(14),
};

struct bcm2835_pll_data {
    const char* name;
    u32 cm_ctrl_reg;
    u32 a2w_ctrl_reg;
    u32 frac_reg;
    u32 ana_reg_base;
    u32 reference_enable_mask;
    u32 lock_mask;

    const struct bcm2835_pll_ana_bits* ana;

    unsigned long min_rate;
    unsigned long max_rate;
    unsigned long max_fb_rate;
};

struct bcm2835_pll_divider_data {
    const char* name;
    const char* source_pll;

    u32 cm_reg;
    u32 a2w_reg;

    u32 load_mask;
    u32 hold_mask;
    u32 fixed_divider;
};

struct bcm2835_clock_data {
    const char* name;

    const char* const* parents;
    int num_mux_parents;

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

struct bcm2835_pll {
    struct clk_hw hw;
    struct bcm2835_cprman* cprman;
    const struct bcm2835_pll_data* data;
};

struct bcm2835_pll_divider {
    struct clk_divider div;
    struct bcm2835_cprman* cprman;
    const struct bcm2835_pll_divider_data* data;
};

struct bcm2835_clock {
    struct clk_hw hw;
    struct bcm2835_cprman* cprman;
    const struct bcm2835_clock_data* data;
};

#define _REGISTER(f, ...)                      \
    {                                          \
        .clk_register = f, .data = __VA_ARGS__ \
    }
#define REGISTER_PLL(...) \
    _REGISTER(&bcm2835_register_pll, &(struct bcm2835_pll_data){__VA_ARGS__})
#define REGISTER_PLL_DIV(...)                \
    _REGISTER(&bcm2835_register_pll_divider, \
              &(struct bcm2835_pll_divider_data){__VA_ARGS__})
#define REGISTER_CLK(...)              \
    _REGISTER(&bcm2835_register_clock, \
              &(struct bcm2835_clock_data){__VA_ARGS__})

static const char* const bcm2835_clock_per_parents[] = {
    "gnd",      "xosc",     "testdebug0", "testdebug1",
    "plla_per", "pllc_per", "plld_per",   "pllh_aux",
};

#define REGISTER_PER_CLK(...)                                             \
    REGISTER_CLK(.num_mux_parents = sizeof(bcm2835_clock_per_parents) /   \
                                    sizeof(bcm2835_clock_per_parents[0]), \
                 .parents = bcm2835_clock_per_parents, __VA_ARGS__)

extern void* boot_params;

static const char* const bcm2835_compat[] = {"brcm,bcm2835-cprman", NULL};

static struct clk_hw* bcm2835_register_pll(struct bcm2835_cprman* cprman,
                                           const void* data);
static struct clk_hw*
bcm2835_register_pll_divider(struct bcm2835_cprman* cprman, const void* data);
static struct clk_hw* bcm2835_register_clock(struct bcm2835_cprman* cprman,
                                             const void* data);

static const struct bcm2835_clk_desc clk_desc_array[] = {
    [BCM2835_PLLC] =
        REGISTER_PLL(.name = "pllc", .cm_ctrl_reg = CM_PLLC,
                     .a2w_ctrl_reg = A2W_PLLC_CTRL, .frac_reg = A2W_PLLC_FRAC,
                     .ana_reg_base = A2W_PLLC_ANA0,
                     .reference_enable_mask = A2W_XOSC_CTRL_PLLC_ENABLE,
                     .lock_mask = CM_LOCK_FLOCKC,

                     .ana = &bcm2835_ana_default,

                     .min_rate = 600000000u, .max_rate = 3000000000u,
                     .max_fb_rate = BCM2835_MAX_FB_RATE),

    [BCM2835_PLLC_PER] =
        REGISTER_PLL_DIV(.name = "pllc_per", .source_pll = "pllc",
                         .cm_reg = CM_PLLC, .a2w_reg = A2W_PLLC_PER,
                         .load_mask = CM_PLLC_LOADPER,
                         .hold_mask = CM_PLLC_HOLDPER, .fixed_divider = 1),

    /* Arasan EMMC clock */
    [BCM2835_CLOCK_EMMC] =
        REGISTER_PER_CLK(.name = "emmc", .ctl_reg = CM_EMMCCTL,
                         .div_reg = CM_EMMCDIV, .int_bits = 4, .frac_bits = 8,
                         .tcnt_mux = 39),

};

static inline void cprman_write(struct bcm2835_cprman* cprman, u32 reg, u32 val)
{
    writel(cprman->regs + reg, CM_PASSWORD | val);
}

static inline u32 cprman_read(struct bcm2835_cprman* cprman, u32 reg)
{
    return readl(cprman->regs + reg);
}

static inline u32
bcm2835_pll_get_prediv_mask(struct bcm2835_cprman* cprman,
                            const struct bcm2835_pll_data* data)
{
    return data->ana->fb_prediv_mask;
}

static long bcm2835_pll_rate_from_divisors(unsigned long parent_rate, u32 ndiv,
                                           u32 fdiv, u32 pdiv)
{
    u64 rate;

    if (pdiv == 0) return 0;

    rate = (u64)parent_rate * ((ndiv << A2W_PLL_FRAC_BITS) + fdiv);
    do_div(rate, pdiv);
    return rate >> A2W_PLL_FRAC_BITS;
}

static unsigned long bcm2835_pll_get_rate(struct clk_hw* hw,
                                          unsigned long parent_rate)
{
    struct bcm2835_pll* pll = list_entry(hw, struct bcm2835_pll, hw);
    struct bcm2835_cprman* cprman = pll->cprman;
    const struct bcm2835_pll_data* data = pll->data;
    u32 a2wctrl = cprman_read(cprman, data->a2w_ctrl_reg);
    u32 ndiv, pdiv, fdiv;
    int using_prediv;

    if (parent_rate == 0) return 0;

    fdiv = cprman_read(cprman, data->frac_reg) & A2W_PLL_FRAC_MASK;
    ndiv = (a2wctrl & A2W_PLL_CTRL_NDIV_MASK) >> A2W_PLL_CTRL_NDIV_SHIFT;
    pdiv = (a2wctrl & A2W_PLL_CTRL_PDIV_MASK) >> A2W_PLL_CTRL_PDIV_SHIFT;
    using_prediv = cprman_read(cprman, data->ana_reg_base + 4) &
                   bcm2835_pll_get_prediv_mask(cprman, data);

    if (using_prediv) {
        ndiv *= 2;
        fdiv *= 2;
    }

    return bcm2835_pll_rate_from_divisors(parent_rate, ndiv, fdiv, pdiv);
}

static const struct clk_ops bcm2835_pll_clk_ops = {
    .recalc_rate = bcm2835_pll_get_rate,
};

static struct clk_hw* bcm2835_register_pll(struct bcm2835_cprman* cprman,
                                           const void* data)
{
    const struct bcm2835_pll_data* pll_data = data;
    struct bcm2835_pll* pll;
    struct clk_init_data init = {0};
    int ret;

    init.parent_names = &cprman->parent_names[0];
    init.num_parents = 1;
    init.name = pll_data->name;
    init.ops = &bcm2835_pll_clk_ops;

    pll = malloc(sizeof(*pll));
    if (!pll) return NULL;

    memset(pll, 0, sizeof(*pll));
    pll->cprman = cprman;
    pll->data = pll_data;

    ret = clk_hw_register(&pll->hw, &init);
    if (ret) {
        free(pll);
        return NULL;
    }

    return &pll->hw;
}

static unsigned long bcm2835_pll_divider_get_rate(struct clk_hw* hw,
                                                  unsigned long parent_rate)
{
    return clk_divider_ops.recalc_rate(hw, parent_rate);
}

static const struct clk_ops bcm2835_pll_divider_clk_ops = {
    .recalc_rate = bcm2835_pll_divider_get_rate,
};

static struct clk_hw*
bcm2835_register_pll_divider(struct bcm2835_cprman* cprman, const void* data)
{
    const struct bcm2835_pll_divider_data* divider_data = data;
    struct bcm2835_pll_divider* divider;
    const char* divider_name;
    struct clk_init_data init = {0};
    int ret;

    divider_name = divider_data->name;

    init.parent_names = &divider_data->source_pll;
    init.num_parents = 1;
    init.name = divider_name;
    init.ops = &bcm2835_pll_divider_clk_ops;

    divider = malloc(sizeof(*divider));
    if (!divider) return NULL;

    memset(divider, 0, sizeof(*divider));
    divider->div.reg = cprman->regs + divider_data->a2w_reg;
    divider->div.shift = A2W_PLL_DIV_SHIFT;
    divider->div.width = A2W_PLL_DIV_BITS;
    divider->div.flags = CLK_DIVIDER_MAX_AT_ZERO;

    divider->cprman = cprman;
    divider->data = divider_data;

    ret = clk_hw_register(&divider->div.hw, &init);
    if (ret) {
        free(divider);
        return NULL;
    }

    return &divider->div.hw;
}

static struct bcm2835_clock* bcm2835_clock_from_hw(struct clk_hw* hw)
{
    return list_entry(hw, struct bcm2835_clock, hw);
}

static long bcm2835_clock_rate_from_divisor(struct bcm2835_clock* clock,
                                            unsigned long parent_rate, u32 div)
{
    const struct bcm2835_clock_data* data = clock->data;
    u64 temp;

    if (data->int_bits == 0 && data->frac_bits == 0) return parent_rate;

    div >>= CM_DIV_FRAC_BITS - data->frac_bits;
    div &= (1 << (data->int_bits + data->frac_bits)) - 1;

    if (div == 0) return 0;

    temp = (u64)parent_rate << data->frac_bits;

    do_div(temp, div);

    return temp;
}

static u8 bcm2835_clock_get_parent(struct clk_hw* hw)
{
    struct bcm2835_clock* clock = bcm2835_clock_from_hw(hw);
    struct bcm2835_cprman* cprman = clock->cprman;
    const struct bcm2835_clock_data* data = clock->data;
    u32 src = cprman_read(cprman, data->ctl_reg);

    return (src & CM_SRC_MASK) >> CM_SRC_SHIFT;
}

static unsigned long bcm2835_clock_get_rate(struct clk_hw* hw,
                                            unsigned long parent_rate)
{
    struct bcm2835_clock* clock = bcm2835_clock_from_hw(hw);
    struct bcm2835_cprman* cprman = clock->cprman;
    const struct bcm2835_clock_data* data = clock->data;
    u32 div;

    if (data->int_bits == 0 && data->frac_bits == 0) return parent_rate;

    div = cprman_read(cprman, data->div_reg);

    return bcm2835_clock_rate_from_divisor(clock, parent_rate, div);
}

static const struct clk_ops bcm2835_clock_clk_ops = {
    .get_parent = bcm2835_clock_get_parent,
    .recalc_rate = bcm2835_clock_get_rate,
};

static struct clk_hw* bcm2835_register_clock(struct bcm2835_cprman* cprman,
                                             const void* data)
{
    const struct bcm2835_clock_data* clock_data = data;
    struct bcm2835_clock* clock;
    struct clk_init_data init = {0};
    const char* parents[1 << CM_SRC_BITS];
    int i, ret;

    for (i = 0; i < clock_data->num_mux_parents; i++) {
        int j;

        parents[i] = clock_data->parents[i];
        for (j = 0;
             j < sizeof(cprman_parent_names) / sizeof(cprman_parent_names[0]);
             j++) {
            if (!strcmp(parents[i], cprman_parent_names[j])) {
                parents[i] = cprman->parent_names[j];
                break;
            }
        }
    }

    memset(&init, 0, sizeof(init));
    init.parent_names = parents;
    init.num_parents = clock_data->num_mux_parents;
    init.name = clock_data->name;

    init.ops = &bcm2835_clock_clk_ops;

    clock = malloc(sizeof(*clock));
    if (!clock) return NULL;

    memset(clock, 0, sizeof(*clock));
    clock->cprman = cprman;
    clock->data = clock_data;

    ret = clk_hw_register(&clock->hw, &init);
    if (ret) {
        free(clock);
        return NULL;
    }

    return &clock->hw;
}

static int fdt_scan_bcm2835_clk(void* blob, unsigned long offset,
                                const char* name, int depth, void* arg)
{
    struct bcm2835_cprman* cprman;
    const size_t asize = sizeof(clk_desc_array) / sizeof(clk_desc_array[0]);
    const struct bcm2835_clk_desc* desc;
    phys_bytes base, size;
    u32 phandle;
    int i, ret;

    if (!of_flat_dt_match(blob, offset, bcm2835_compat)) return 0;

    phandle = fdt_get_phandle(blob, offset);

    cprman =
        malloc(sizeof(struct bcm2835_cprman) + asize * sizeof(struct clk_hw*));
    if (!cprman) return 1;

    memset(cprman, 0,
           sizeof(struct bcm2835_cprman) + asize * sizeof(struct clk_hw*));

    ret = of_address_parse_one(blob, offset, 0, &base, &size);
    if (ret < 0) goto out_free;

    cprman->regs = mm_map_phys(SELF, base, size, MMP_IO);
    if (cprman->regs == MAP_FAILED) goto out_free;

    memcpy(cprman->parent_names, cprman_parent_names,
           sizeof(cprman_parent_names));
    of_clk_parent_fill(blob, offset, cprman->parent_names,
                       sizeof(cprman_parent_names) /
                           sizeof(cprman_parent_names[0]));

    if (!cprman->parent_names[0]) return 0;

    cprman->onecell.num = asize;
    for (i = 0; i < asize; i++) {
        desc = &clk_desc_array[i];
        if (desc->clk_register && desc->data) {
            cprman->onecell.hws[i] = desc->clk_register(cprman, desc->data);
        }
    }

    ret = clk_add_hw_provider(phandle, clk_hw_onecell_get, &cprman->onecell);
    if (ret) return 0;

    return 1;

out_free:
    free(cprman);
    return 0;
}

void bcm2835_clk_scan(void)
{
    of_scan_fdt(fdt_scan_bcm2835_clk, NULL, boot_params);
}
