#include <lyos/const.h>
#include <lyos/list.h>
#include <stddef.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#include "clk.h"

#define to_clk_fixed_rate(hw) list_entry(hw, struct clk_fixed_rate, hw)

static unsigned long clk_fixed_rate_recalc_rate(struct clk_hw* hw,
                                                unsigned long parent_rate)
{
    return to_clk_fixed_rate(hw)->fixed_rate;
}

static const struct clk_ops clk_fixed_rate_ops = {
    .recalc_rate = clk_fixed_rate_recalc_rate,
};

struct clk_hw* clk_hw_register_fixed_rate(const char* name,
                                          const char* parent_name,
                                          unsigned long fixed_rate)
{
    struct clk_fixed_rate* fixed;
    struct clk_hw* hw;
    struct clk_init_data init = {};
    int ret;

    fixed = malloc(sizeof(*fixed));
    if (!fixed) return NULL;

    memset(fixed, 0, sizeof(*fixed));

    init.name = name;
    init.ops = &clk_fixed_rate_ops;
    init.parent_names = parent_name ? &parent_name : NULL;
    init.num_parents = 0;
    if (parent_name) init.num_parents = 1;

    fixed->fixed_rate = fixed_rate;

    hw = &fixed->hw;
    ret = clk_hw_register(hw, &init);
    if (ret) return NULL;

    return hw;
}

#if CONFIG_OF

extern void* boot_params;

static const char* const fixed_rate_compat[] = {"fixed-clock", NULL};

static int fdt_scan_fixed_rate_clk(void* blob, unsigned long offset,
                                   const char* name, int depth, void* arg)
{
    const u32* prop;
    u32 rate;
    const char* clk_name = name;
    struct clk_hw* hw;
    u32 phandle;

    if (!of_flat_dt_match(blob, offset, fixed_rate_compat)) return 0;

    prop = fdt_getprop(blob, offset, "clock-frequency", NULL);
    if (!prop) return 0;

    rate = of_read_number(prop, 1);

    prop = fdt_getprop(blob, offset, "clock-output-names", NULL);
    if (prop) clk_name = (const char*)prop;

    phandle = fdt_get_phandle(blob, offset);

    hw = clk_hw_register_fixed_rate(clk_name, NULL, rate);
    if (!hw) return 0;

    clk_add_hw_provider(phandle, clk_hw_simple_get, hw);

    return 0;
}

#endif

void fixed_rate_scan(void)
{
#if CONFIG_OF
    of_scan_fdt(fdt_scan_fixed_rate_clk, NULL, boot_params);
#endif
}
