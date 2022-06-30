#ifndef _CLK_CLK_H_
#define _CLK_CLK_H_

struct clk;
struct clk_core;
struct clk_hw;
struct clkspec;

struct clk_ops {
    unsigned long (*recalc_rate)(struct clk_hw* hw, unsigned long parent_rate);
    u8 (*get_parent)(struct clk_hw* hw);
    int (*init)(struct clk_hw* hw);
};

struct clk_init_data {
    const char* name;
    const struct clk_ops* ops;
    const char* const* parent_names;
    u8 num_parents;
};

struct clk_hw {
    struct clk_core* core;
    struct clk* clk;
};

struct clk_hw_onecell_data {
    unsigned int num;
    struct clk_hw* hws[];
};

struct clk_fixed_rate {
    struct clk_hw hw;
    unsigned long fixed_rate;
};

struct clk_divider {
    struct clk_hw hw;
    void* reg;
    u8 shift;
    u8 width;
    u8 flags;
};

#define clk_div_mask(width) ((1 << (width)) - 1)
#define to_clk_divider(_hw) list_entry(_hw, struct clk_divider, hw)

#define CLK_DIVIDER_ONE_BASED     BIT(0)
#define CLK_DIVIDER_POWER_OF_TWO  BIT(1)
#define CLK_DIVIDER_ALLOW_ZERO    BIT(2)
#define CLK_DIVIDER_HIWORD_MASK   BIT(3)
#define CLK_DIVIDER_ROUND_CLOSEST BIT(4)
#define CLK_DIVIDER_READ_ONLY     BIT(5)
#define CLK_DIVIDER_MAX_AT_ZERO   BIT(6)
#define CLK_DIVIDER_BIG_ENDIAN    BIT(7)

extern const struct clk_ops clk_divider_ops;

int clk_hw_register(struct clk_hw* hw, const struct clk_init_data* init);

int clk_add_hw_provider(u32 phandle,
                        struct clk_hw* (*get)(struct clkspec* clkspec,
                                              void* data),
                        void* data);

struct clk_hw* clk_hw_simple_get(struct clkspec* clkspec, void* data);
struct clk_hw* clk_hw_onecell_get(struct clkspec* clkspec, void* data);

#if CONFIG_OF
const char* of_clk_get_parent_name(void* blob, unsigned long offset, int index);
int of_clk_parent_fill(void* blob, unsigned long offset, const char** parents,
                       unsigned int size);

#endif

#endif
