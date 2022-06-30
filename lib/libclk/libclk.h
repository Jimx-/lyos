#ifndef _LIBCLK_H_
#define _LIBCLK_H_

#include <lyos/types.h>

typedef int clk_id_t;

#define CLKSPEC_PARAMS 8

struct clkspec {
    unsigned int provider;
    int param_count;
    u32 param[CLKSPEC_PARAMS];
};

struct clk_get_request {
    clk_id_t clk_id;
    struct clkspec clkspec;
};

struct clk_common_request {
    clk_id_t clk_id;
    u64 rate;
};

clk_id_t clk_get(struct clkspec* clkspec);

#if CONFIG_OF
struct of_phandle_args;

clk_id_t clk_get_of(struct of_phandle_args* args);
#endif

unsigned long clk_get_rate(clk_id_t clk);

#endif
