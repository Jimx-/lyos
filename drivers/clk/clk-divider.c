#include <sys/types.h>
#include <lyos/const.h>
#include <lyos/list.h>
#include <stddef.h>
#include <asm/io.h>
#include <asm/div64.h>

#include "clk.h"

static inline u32 clk_div_readl(struct clk_divider* divider)
{
    return readl(divider->reg);
}

static unsigned int _get_div(unsigned int val, unsigned long flags, u8 width)
{
    if (flags & CLK_DIVIDER_ONE_BASED) return val;
    if (flags & CLK_DIVIDER_POWER_OF_TWO) return 1 << val;
    if (flags & CLK_DIVIDER_MAX_AT_ZERO)
        return val ? val : clk_div_mask(width) + 1;
    return val + 1;
}

unsigned long divider_recalc_rate(struct clk_hw* hw, unsigned long parent_rate,
                                  unsigned int val, unsigned long flags,
                                  unsigned long width)
{
    unsigned int div;
    u64 rate;

    div = _get_div(val, flags, width);
    if (!div) return parent_rate;

    rate = parent_rate + div - 1;
    do_div(rate, div);

    return rate;
}

static unsigned long clk_divider_recalc_rate(struct clk_hw* hw,
                                             unsigned long parent_rate)
{
    struct clk_divider* divider = to_clk_divider(hw);
    unsigned int val;

    val = clk_div_readl(divider) >> divider->shift;
    val &= clk_div_mask(divider->width);

    return divider_recalc_rate(hw, parent_rate, val, divider->flags,
                               divider->width);
}

const struct clk_ops clk_divider_ops = {
    .recalc_rate = clk_divider_recalc_rate,
};
