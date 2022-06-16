#ifndef _CLK_CLK_H_
#define _CLK_CLK_H_

struct clk_hw {};

struct clk_hw_onecell_data {
    unsigned int num;
    struct clk_hw* hws[];
};

#endif
