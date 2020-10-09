#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <lyos/const.h>
#include <lyos/sysutils.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/fs.h>
#include <lyos/vm.h>
#include <asm/atomic.h>
#include <signal.h>
#include <errno.h>

#include "region.h"
#include "proto.h"
#include "global.h"

static int shared_map_pt_flags(const struct vir_region* vr) { return 0; }

static int shared_map_unreference(struct phys_region* pr)
{
    return anon_map_ops.rop_unreference(pr);
}

void shared_set_source(struct vir_region* vr, endpoint_t ep,
                       struct vir_region* src)
{}

const struct region_operations shared_map_ops = {
    .rop_pt_flags = shared_map_pt_flags,

    .rop_unreference = shared_map_unreference,
};
