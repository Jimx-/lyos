#include <lyos/config.h>
#include <lyos/types.h>
#include <lyos/ipc.h>
#include <lyos/const.h>
#include <lyos/service.h>
#include <libsysfs/libsysfs.h>
#include <string.h>
#include <errno.h>

#include <libof/libof.h>

#include "libclk.h"

endpoint_t __clk_endpoint = NO_TASK;

static int clk_sendrec(int function, MESSAGE* msg)
{
    int retval;
    u32 v;

    if (__clk_endpoint == NO_TASK) {
        retval = sysfs_retrieve_u32("services.clk.endpoint", &v);
        if (retval) return retval;

        __clk_endpoint = (endpoint_t)v;
    }

    return send_recv(function, __clk_endpoint, msg);
}

clk_id_t clk_get(struct clkspec* clkspec)
{
    MESSAGE m;
    struct clk_get_request* req = (struct clk_get_request*)m.MSG_PAYLOAD;

    memset(&m, 0, sizeof(m));
    m.type = CLK_GET;
    req->clkspec = *clkspec;

    clk_sendrec(BOTH, &m);

    return req->clk_id;
}

#if CONFIG_OF
clk_id_t clk_get_of(struct of_phandle_args* args)
{
    struct clkspec clkspec;
    int i;

    if (args->args_count > CLKSPEC_PARAMS) return -EINVAL;

    clkspec.provider = args->phandle;
    clkspec.param_count = args->args_count;
    for (i = 0; i < args->args_count; i++)
        clkspec.param[i] = args->args[i];

    return clk_get(&clkspec);
}
#endif

unsigned long clk_get_rate(clk_id_t clk)
{
    MESSAGE m;
    struct clk_common_request* req = (struct clk_common_request*)m.MSG_PAYLOAD;

    memset(&m, 0, sizeof(m));
    m.type = CLK_GET_RATE;
    req->clk_id = clk;

    clk_sendrec(BOTH, &m);

    return (unsigned long)req->rate;
}
