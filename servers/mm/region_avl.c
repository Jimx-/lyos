#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/fs.h>
#include <lyos/vm.h>
#include <atomic.h>
#include "signal.h"
#include "errno.h"
#include "region.h"
#include "global.h"
#include "proto.h"
#include "const.h"
#include "cache.h"
#include "file.h"

PRIVATE int region_key_node_comp(void* key, struct avl_node* node)
{
    struct vir_region* r1 = (struct vir_region*) key;
    struct vir_region* r2 = avl_entry(node, struct vir_region, avl);

    vir_bytes vaddr1 = (vir_bytes) r1->vir_addr;
    vir_bytes vaddr2 = (vir_bytes) r2->vir_addr;

    if (vaddr1 < vaddr2) return -1;
    else if (vaddr1 > vaddr2) return 1;
    return 0;
}

PRIVATE int region_node_node_comp(struct avl_node* node1, struct avl_node* node2)
{
    struct vir_region* r1 = avl_entry(node1, struct vir_region, avl);
    struct vir_region* r2 = avl_entry(node2, struct vir_region, avl);

    vir_bytes vaddr1 = (vir_bytes) r1->vir_addr;
    vir_bytes vaddr2 = (vir_bytes) r2->vir_addr;

    if (vaddr1 < vaddr2) return -1;
    else if (vaddr1 > vaddr2) return 1;
    return 0;
}


PUBLIC void region_init_avl(struct mm_struct* mm)
{
    INIT_AVL_ROOT(&mm->mem_avl, region_key_node_comp, region_node_node_comp);
}

PUBLIC struct vir_region * region_lookup(struct mmproc * mmp, vir_bytes addr)
{
    struct avl_node* node = mmp->active_mm->mem_avl.node;

    while (node) {
    	struct vir_region * vr = avl_entry(node, struct vir_region, avl);
        
        if (addr >= vr->vir_addr && addr < vr->vir_addr + vr->length) {
            return vr;
        } else if (addr < (vir_bytes)(vr->vir_addr)) {
        	node = node->left;
        } else if (addr > (vir_bytes)(vr->vir_addr)) {
        	node = node->right;
        }
    }

    return NULL;
}

PUBLIC void region_avl_start_iter(struct avl_root* root, struct avl_iter* iter, void* key, int flags)
{
    avl_start_iter(root, iter, key, flags);
}

PUBLIC struct vir_region* region_avl_get_iter(struct avl_iter* iter)
{
    struct avl_node* node = avl_get_iter(iter);
    if (!node) return NULL;
    return avl_entry(node, struct vir_region, avl);
}

PUBLIC void region_avl_inc_iter(struct avl_iter* iter)
{
    avl_inc_iter(iter);
}

PUBLIC void region_avl_dec_iter(struct avl_iter* iter)
{
    avl_dec_iter(iter);
}
