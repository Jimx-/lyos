#include <lyos/ipc.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <lyos/idr.h>
#include <lyos/list.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>

#include <libclk/libclk.h>

#include <libfdt/libfdt.h>
#include <libof/libof.h>

#include "clk.h"
#include "proto.h"

#define NAME "clk"

#if CONFIG_OF
void* boot_params;
#endif

struct clk_parent_map {
    const char* name;
    struct clk_core* core;
};

struct clk_core {
    int index;
    const char* name;
    const struct clk_ops* ops;
    struct clk_hw* hw;
    struct clk_core* parent;
    struct clk_parent_map* parents;
    u8 num_parents;
    unsigned long rate;
    struct list_head children;
    struct list_head child_node;
    struct list_head clks;
};

struct clk {
    int index;
    struct clk_core* core;
    struct list_head clks_node;
};

static struct list_head clk_root_list;

static struct idr clk_idr;

static inline struct clk* clk_find_by_id(clk_id_t id)
{
    return idr_find(&clk_idr, (unsigned long)id);
}

static struct clk_core* clk_core_lookup_subtree(struct clk_core* core,
                                                const char* name)
{
    struct clk_core *child, *ret;

    if (!strcmp(core->name, name)) return core;

    list_for_each_entry(child, &core->children, child_node)
    {
        ret = clk_core_lookup_subtree(child, name);
        if (ret) return ret;
    }

    return NULL;
}

static struct clk_core* clk_core_lookup(const char* name)
{
    struct clk_core* clk;
    struct clk_core* ret;

    if (!name) return NULL;

    list_for_each_entry(clk, &clk_root_list, child_node)
    {
        ret = clk_core_lookup_subtree(clk, name);
        if (ret) return ret;
    }

    return NULL;
}

static unsigned long clk_core_get_rate(struct clk_core* core)
{
    if (!core) return 0;

    if (!core->num_parents || core->parent) return core->rate;

    return 0;
}

static struct clk* alloc_clk(struct clk_core* core)
{
    struct clk* clk;
    int index;

    clk = malloc(sizeof(*clk));
    if (!clk) return NULL;

    memset(clk, 0, sizeof(*clk));
    clk->core = core;

    index = idr_alloc(&clk_idr, clk, 1, 0);
    if (index < 0) {
        free(clk);
        return NULL;
    }
    clk->index = index;

    return clk;
}

static void free_clk(struct clk* clk)
{
    idr_remove(&clk_idr, clk->index);
    free(clk);
}

struct clk* clk_hw_create_clk(struct clk_hw* hw)
{
    struct clk* clk;
    struct clk_core* core;

    if (!hw) return NULL;

    core = hw->core;
    clk = alloc_clk(core);
    if (!clk) return NULL;

    list_add(&clk->clks_node, &core->clks);
    return clk;
}

static int clk_copy_name(const char** dst_p, const char* src, int must_exist)
{
    const char* dst;

    if (!src) {
        if (must_exist) return EINVAL;
        return 0;
    }

    *dst_p = dst = strdup(src);
    if (!dst) return ENOMEM;

    return 0;
}

static int clk_core_populate_parent_map(struct clk_core* core,
                                        const struct clk_init_data* init)
{
    u8 num_parents = init->num_parents;
    const char* const* parent_names = init->parent_names;
    struct clk_parent_map *parents, *parent;
    int i, ret = 0;

    if (!num_parents) return 0;

    parents = calloc(num_parents, sizeof(*parents));
    core->parents = parents;
    if (!parents) return ENOMEM;

    for (i = 0, parent = parents; i < num_parents; i++, parent++) {
        if (parent_names) {
            ret = clk_copy_name(&parent->name, parent_names[i], TRUE);
        }

        if (ret) {
            do {
                free((void*)parents[i].name);
            } while (--i >= 0);
            free(parents);

            return ret;
        }
    }

    return 0;
}

static void clk_core_free_parent_map(struct clk_core* core)
{
    int i = core->num_parents;

    if (!i) return;

    while (--i >= 0)
        free((void*)core->parents[i].name);

    free(core->parents);
}

static void clk_core_fill_parent_index(struct clk_core* core, u8 index)
{
    struct clk_parent_map* entry = &core->parents[index];
    struct clk_core* parent = NULL;

    if (entry->name) parent = clk_core_lookup(entry->name);

    if (parent) entry->core = parent;
}

static struct clk_core* clk_core_get_parent_by_index(struct clk_core* core,
                                                     u8 index)
{
    if (!core || index >= core->num_parents || !core->parents) return NULL;

    if (!core->parents[index].core) clk_core_fill_parent_index(core, index);

    return core->parents[index].core;
}

static struct clk_core* clk_init_parent(struct clk_core* core)
{
    u8 index = 0;

    if (core->num_parents > 1 && core->ops->get_parent)
        index = core->ops->get_parent(core->hw);

    return clk_core_get_parent_by_index(core, index);
}

static int clk_core_init(struct clk_core* core)
{
    struct clk_core* parent;
    unsigned long rate;
    int ret;

    if (!core) return EINVAL;

    if (clk_core_lookup(core->name)) return EEXIST;

    if (core->num_parents > 1 && !core->ops->get_parent) return EINVAL;

    if (core->ops->init) {
        ret = core->ops->init(core->hw);
        if (ret) return ret;
    }

    parent = core->parent = clk_init_parent(core);

    if (parent) {
        list_add(&core->child_node, &parent->children);
    } else if (!core->num_parents) {
        list_add(&core->child_node, &clk_root_list);
    }

    if (core->ops->recalc_rate)
        rate = core->ops->recalc_rate(core->hw, clk_core_get_rate(parent));
    else if (parent)
        rate = parent->rate;
    else
        rate = 0;
    core->rate = rate;

    if (ret) list_del(&core->child_node);

    return ret;
}

int clk_hw_register(struct clk_hw* hw, const struct clk_init_data* init)
{
    struct clk_core* core;
    int ret;

    core = malloc(sizeof(*core));
    if (!core) return ENOMEM;

    memset(core, 0, sizeof(*core));

    core->name = strdup(init->name);
    if (!core->name) {
        ret = ENOMEM;
        goto free_core;
    }

    if (!init->ops) {
        ret = EINVAL;
        goto free_name;
    }
    core->ops = init->ops;

    core->hw = hw;
    core->num_parents = init->num_parents;
    hw->core = core;

    ret = clk_core_populate_parent_map(core, init);
    if (ret) goto free_name;

    INIT_LIST_HEAD(&core->clks);
    INIT_LIST_HEAD(&core->children);

    hw->clk = alloc_clk(core);
    if (!hw->clk) {
        ret = ENOMEM;
        goto free_parent_map;
    }

    list_add(&hw->clk->clks_node, &core->clks);

    ret = clk_core_init(core);
    if (!ret) return 0;

    list_del(&hw->clk->clks_node);
    free_clk(hw->clk);
    hw->clk = NULL;

free_parent_map:
    clk_core_free_parent_map(core);
free_name:
    free((void*)core->name);
free_core:
    free(core);
    return ret;
}

struct clk_provider {
    struct list_head link;

    u32 phandle;
    struct clk_hw* (*get_hw)(struct clkspec* clkspec, void* data);
    void* data;
};

static DEF_LIST(clk_providers);

struct clk_hw* clk_hw_simple_get(struct clkspec* clkspec, void* data)
{
    return data;
}

struct clk_hw* clk_hw_onecell_get(struct clkspec* clkspec, void* data)
{
    struct clk_hw_onecell_data* hw_data = data;
    unsigned int idx = clkspec->param[0];

    if (idx >= hw_data->num) return NULL;

    return hw_data->hws[idx];
}

int clk_add_hw_provider(u32 phandle,
                        struct clk_hw* (*get)(struct clkspec* clkspec,
                                              void* data),
                        void* data)
{
    struct clk_provider* cp;

    if (!phandle) return 0;

    cp = malloc(sizeof(*cp));
    if (!cp) return ENOMEM;

    cp->phandle = phandle;
    cp->data = data;
    cp->get_hw = get;

    list_add(&cp->link, &clk_providers);

    return 0;
}

static struct clk_hw* clk_get_hw_from_clkspec(struct clkspec* clkspec)
{
    struct clk_provider* cp;
    struct clk_hw* hw = NULL;

    if (!clkspec) return NULL;

    list_for_each_entry(cp, &clk_providers, link)
    {
        if (cp->phandle == clkspec->provider) {
            hw = cp->get_hw(clkspec, cp->data);
            if (hw) break;
        }
    }

    return hw;
}

#if CONFIG_OF
const char* of_clk_get_parent_name(void* blob, unsigned long offset, int index)
{
    struct of_phandle_args clkspec;
    const char* clk_name;
    int ret;

    ret = of_parse_phandle_with_args(blob, offset, "clocks", "#clock-cells",
                                     index, &clkspec);
    if (ret) return NULL;

    index = clkspec.args_count ? clkspec.args[0] : 0;

    if (of_property_read_string_index(
            blob, clkspec.offset, "clock-output-names", index, &clk_name) < 0) {
        return NULL;
    }

    return clk_name;
}

int of_clk_parent_fill(void* blob, unsigned long offset, const char** parents,
                       unsigned int size)
{
    unsigned int i = 0;

    while (i < size &&
           (parents[i] = of_clk_get_parent_name(blob, offset, i)) != NULL)
        i++;

    return i;
}
#endif

static int clk_init(void)
{
    struct sysinfo* sysinfo;

    printl(NAME ": Common clock framework driver is running.\n");

    INIT_LIST_HEAD(&clk_root_list);

    idr_init(&clk_idr);

#if CONFIG_OF
    get_sysinfo(&sysinfo);
    boot_params = sysinfo->boot_params;
#endif

    fixed_rate_scan();

#if CONFIG_CLK_BCM2835
    bcm2835_clk_scan();
#endif

    return 0;
}

clk_id_t __do_clk_get(struct clkspec* clkspec)
{
    struct clk_hw* hw;
    struct clk* clk;

    hw = clk_get_hw_from_clkspec(clkspec);
    if (!hw) return -EINVAL;

    clk = clk_hw_create_clk(hw);
    if (!clk) return -ENOMEM;

    return clk->index;
}

void do_clk_get(MESSAGE* msg)
{
    struct clk_get_request* req = (struct clk_get_request*)msg->MSG_PAYLOAD;

    req->clk_id = __do_clk_get(&req->clkspec);
}

void do_clk_get_rate(MESSAGE* msg)
{
    struct clk_common_request* req =
        (struct clk_common_request*)msg->MSG_PAYLOAD;
    struct clk* clk;
    u64 rate = 0;

    clk = clk_find_by_id(req->clk_id);
    if (!clk) goto out;

    rate = clk_core_get_rate(clk->core);

out:
    req->rate = rate;
}

int main()
{
    serv_register_init_fresh_callback(clk_init);
    serv_init();

    while (TRUE) {
        MESSAGE msg;

        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;
        int msgtype = msg.type;

        switch (msgtype) {
        case CLK_GET:
            do_clk_get(&msg);
            break;
        case CLK_GET_RATE:
            do_clk_get_rate(&msg);
            break;
        default:
            msg.RETVAL = ENOSYS;
            break;
        }

        msg.type = SYSCALL_RET;
        send_recv(SEND_NONBLOCK, src, &msg);
    }

    return 0;
}
