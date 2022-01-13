#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/driver.h>
#include <asm/byteorder.h>
#include <errno.h>
#include <lyos/portio.h>
#include <lyos/interrupt.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/pci_utils.h>
#include <sys/mman.h>
#include <asm/pci.h>
#include <lyos/kref.h>

#include <libblockdriver/libblockdriver.h>
#include <libvirtio/libvirtio.h>
#include <libsysfs/libsysfs.h>

#include "nvme.h"

#define MAX_THREADS 4

#define IO_QUEUE_DEPTH 1024

static const char* name = "nvme-pci";
static int instance;
static int open_count = 0;

static void* nvme_bar;
static u32* nvme_dbs;
static uint64_t nvme_bar_base;
static unsigned long nvme_bar_map_size;

static u64 ctrl_cap;
static u32 ctrl_config;
static u32 max_hw_sectors;
static u32 ctrl_page_size;
static int queue_depth;
static u32 db_stride;

static int pin_irq_hook;

static u32 cqe_status[MAX_THREADS];
static union nvme_result cqe_result[MAX_THREADS];

struct nvme_queue {
    u16 q_depth;
    u16 cq_vector;
    u16 sq_tail;
    u16 last_sq_tail;
    u16 cq_head;
    u16 qid;
    u8 cq_phase;
    u8 sqes;

    void* sq_cmds;
    volatile struct nvme_completion* cqes;
    phys_bytes sq_phys;
    phys_bytes cq_phys;
    u32* q_db;
};

static struct nvme_queue* nvme_queues;
static unsigned int queue_count;
static unsigned int online_queues;

struct nvme_ns {
    struct list_head list;
    struct kref kref;

    u32 nsid;
    u64 capacity;
    int lba_shift;

    struct part_info primary[NR_PRIM_PER_DRIVE];
    struct part_info logical[NR_SUB_PER_DRIVE];
};

static struct list_head namespaces;
#define NSID_OF_DEV(dev) (dev / NR_SUB_PER_DRIVE)

struct nvme_iod {
    void* prp_list_buf;
    size_t prp_list_size;
};

static class_id_t nvme_class;
static device_id_t nvme_dev_id;

static int nvme_pci_open(dev_t minor, int access);
static int nvme_pci_close(dev_t minor);
static ssize_t nvme_pci_rdwt(dev_t minor, int do_write, loff_t pos,
                             endpoint_t endpoint, const struct iovec_grant* iov,
                             size_t count);
static struct part_info* nvme_pci_part(dev_t device);
static void nvme_pci_intr(unsigned mask);

static struct blockdriver nvme_pci_driver = {
    .bdr_open = nvme_pci_open,
    .bdr_close = nvme_pci_close,
    .bdr_readwrite = nvme_pci_rdwt,
    .bdr_part = nvme_pci_part,
    .bdr_intr = nvme_pci_intr,
};

#define SQ_SIZE(q) ((q)->q_depth << (q)->sqes)
#define CQ_SIZE(q) ((q)->q_depth * sizeof(struct nvme_completion))

static inline u64 nvme_lba_to_sect(struct nvme_ns* ns, u64 lba)
{
    return lba << (ns->lba_shift - SECTOR_SIZE_SHIFT);
}

static u32 nvme_pci_read32(__le32* addr) { return *(volatile u32*)addr; }

static void nvme_pci_write32(__le32* addr, u32 val)
{
    *(volatile u32*)addr = val;
}

static u64 nvme_pci_read64(void* addr)
{
    u32 low, high;
    low = nvme_pci_read32(addr);
    high = nvme_pci_read32(addr + 4);
    return ((u64)high << 32) | low;
}

static void nvme_pci_write64(void* addr, u64 val)
{
    nvme_pci_write32(addr, (u32)val);
    nvme_pci_write32(addr + 4, val >> 32);
}

static unsigned int max_io_queues(void) { return MAX_THREADS; }

static unsigned int max_queue_count(void) { return 1 + max_io_queues(); }

static unsigned long db_bar_size(unsigned nr_io_queues)
{
    return NVME_REG_DBS + ((nr_io_queues + 1) * 8 * db_stride);
}

static struct nvme_ns* nvme_find_get_ns(unsigned int nsid)
{
    struct nvme_ns* ns;

    list_for_each_entry(ns, &namespaces, list)
    {
        if (ns->nsid == nsid) {
            kref_get(&ns->kref);
            return ns;
        }

        if (ns->nsid > nsid) break;
    }

    return NULL;
}

static void nvme_free_ns(struct kref* kref)
{
    struct nvme_ns* ns = list_entry(kref, struct nvme_ns, kref);
    free(ns);
}

static void nvme_put_ns(struct nvme_ns* ns)
{
    kref_put(&ns->kref, nvme_free_ns);
}

static void nvme_remove_ns(struct nvme_ns* ns)
{
    list_del(&ns->list);
    nvme_put_ns(ns);
}

static int nvme_pci_open(dev_t minor, int access)
{
    open_count++;
    return 0;
}

static int nvme_pci_close(dev_t minor)
{
    open_count--;
    return 0;
}

static struct part_info* nvme_pci_part(dev_t device)
{
    unsigned nsid = NSID_OF_DEV(device);
    struct nvme_ns* ns = nvme_find_get_ns(nsid);

    if (!ns) return NULL;

    int part_no = MINOR(device) % NR_SUB_PER_DRIVE;
    struct part_info* part = part_no < NR_PRIM_PER_DRIVE
                                 ? &ns->primary[part_no]
                                 : &ns->logical[part_no - NR_PRIM_PER_DRIVE];

    return part;
}

static int nvme_pci_remap_bar(unsigned long size)
{
    if (nvme_bar) munmap(nvme_bar, nvme_bar_map_size);

    nvme_bar = mm_map_phys(SELF, (void*)(uintptr_t)nvme_bar_base, size);
    if (!nvme_bar) {
        nvme_bar_map_size = 0;
        nvme_dbs = NULL;
        return ENOMEM;
    }

    nvme_bar_map_size = size;
    nvme_dbs = nvme_bar + NVME_REG_DBS;

    return 0;
}

static void nvme_pci_write_sq_db(struct nvme_queue* nvmeq, int write_sq)
{
    if (!write_sq) {
        u16 next_tail = nvmeq->sq_tail + 1;

        if (next_tail == nvmeq->q_depth) next_tail = 0;
        if (next_tail != nvmeq->last_sq_tail) return;
    }

    nvme_pci_write32(nvmeq->q_db, nvmeq->sq_tail);
    nvmeq->last_sq_tail = nvmeq->sq_tail;
}

static void nvme_pci_submit_sq_cmd(struct nvme_queue* nvmeq,
                                   struct nvme_command* cmd, int write_sq)
{
    memcpy(nvmeq->sq_cmds + (nvmeq->sq_tail << nvmeq->sqes), cmd, sizeof(*cmd));
    if (++nvmeq->sq_tail == nvmeq->q_depth) nvmeq->sq_tail = 0;
    nvme_pci_write_sq_db(nvmeq, write_sq);
}

static int nvme_fill_buffers(struct iovec_grant* iov, struct umap_phys* phys,
                             size_t count, endpoint_t endpoint)
{
    int i, retval;

    for (i = 0; i < count; i++) {
        if (endpoint == SELF) {
            if ((retval = umap(endpoint, UMT_VADDR, (vir_bytes)iov[i].iov_addr,
                               iov[i].iov_len, &phys[i].phys_addr)) != 0)
                return -retval;
        } else {
            if ((retval = umap(endpoint, UMT_GRANT, iov[i].iov_grant,
                               iov[i].iov_len, &phys[i].phys_addr)) != 0)
                return -retval;
        }

        phys[i].size = iov[i].iov_len;
    }

    return 0;
}

static void nvme_unmap_data(struct nvme_iod* iod)
{
    if (iod->prp_list_buf && iod->prp_list_size)
        munmap(iod->prp_list_buf, iod->prp_list_size);
}

static int nvme_setup_prp_simple(struct nvme_rw_command* cmd,
                                 struct umap_phys* buf)
{
    phys_bytes offset = buf->phys_addr % ctrl_page_size;
    unsigned int first_prp_len = ctrl_page_size - offset;

    cmd->dptr.prp1 = cpu_to_le64((u64)buf->phys_addr);
    if (buf->size > first_prp_len)
        cmd->dptr.prp2 = cpu_to_le64((u64)(buf->phys_addr + first_prp_len));

    return 0;
}

static int nvme_npages(unsigned int size)
{
    unsigned int nprps = (size + ctrl_page_size) / ctrl_page_size;
    return ((nprps << 3) + ARCH_PG_SIZE - 8) / (ARCH_PG_SIZE - 8);
}

static int nvme_setup_prps(struct nvme_command* cmd, struct umap_phys* bufs,
                           size_t count, size_t size, struct nvme_iod* iod)
{
    u32 page_size = ctrl_page_size;
    int dma_len = bufs->size;
    phys_bytes dma_addr = bufs->phys_addr;
    int offset = dma_addr % page_size;
    __le64* prp_list = NULL;
    phys_bytes prp_phys, first_phys;
    int i, retval;

    if (size <= page_size - offset) {
        first_phys = 0;
        goto out;
    }
    size -= (page_size - offset);

    dma_len -= (page_size - offset);
    if (dma_len) {
        dma_addr += (page_size - offset);
    } else {
        bufs++;
        dma_addr = bufs->phys_addr;
        dma_len = bufs->size;
    }

    if (size <= page_size) {
        first_phys = dma_addr;
        goto out;
    }

    iod->prp_list_size = ARCH_PG_SIZE * nvme_npages(size);
    iod->prp_list_buf =
        mmap(NULL, iod->prp_list_size, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (iod->prp_list_buf == MAP_FAILED) {
        iod->prp_list_buf = NULL;
        iod->prp_list_size = 0;
        return -ENOMEM;
    }

    if ((retval = umap(SELF, UMT_VADDR, (vir_bytes)iod->prp_list_buf,
                       iod->prp_list_size, &prp_phys)) != 0)
        return -retval;

    prp_list = iod->prp_list_buf;
    first_phys = prp_phys;
    i = 0;
    for (;;) {
        if (i == page_size >> 3) {
            __le64* old_prp_list = prp_list;
            prp_list = (__le64*)((char*)prp_list + page_size);
            prp_phys += page_size;
            prp_list[0] = old_prp_list[i - 1];
            old_prp_list[i - 1] = cpu_to_le64((u64)prp_phys);
            i = 1;
        }
        prp_list[i++] = cpu_to_le64((u64)dma_addr);
        dma_len -= page_size;
        dma_addr += page_size;

        if (size <= page_size) break;
        size -= page_size;
        if (dma_len > 0) continue;

        bufs++;
        dma_addr = bufs->phys_addr;
        dma_len = bufs->size;
    }

out:
    cmd->rw.dptr.prp1 = cpu_to_le64((u64)bufs->phys_addr);
    cmd->rw.dptr.prp2 = cpu_to_le64((u64)first_phys);

    return 0;
}

static int nvme_map_data(struct nvme_command* cmd, struct umap_phys* bufs,
                         size_t count, size_t size, struct nvme_iod* iod)
{
    int retval;

    memset(iod, 0, sizeof(*iod));

    if (count == 1) {
        phys_bytes offset = bufs->phys_addr % ctrl_page_size;

        if (offset + bufs->size <= ctrl_page_size * 2) {
            return nvme_setup_prp_simple(&cmd->rw, bufs);
        }
    }

    retval = nvme_setup_prps(cmd, bufs, count, size, iod);
    if (retval) nvme_unmap_data(iod);

    return retval;
}

static int nvme_submit_cmd(struct nvme_queue* nvmeq, struct nvme_command* cmd,
                           void* buffer, unsigned bufflen,
                           union nvme_result* result)
{
    struct iovec_grant iov;
    struct umap_phys phys;
    blockdriver_worker_id_t tid;
    struct nvme_iod iod;
    int retval;

    tid = blockdriver_async_worker_id();
    cmd->common.command_id = (u16)tid;

    if (buffer && bufflen) {
        iov.iov_addr = (vir_bytes)buffer;
        iov.iov_len = bufflen;

        retval = nvme_fill_buffers(&iov, &phys, 1, SELF);
        if (retval) return retval;

        retval = nvme_map_data(cmd, &phys, 1, bufflen, &iod);
        if (retval) return retval;
    }

    nvme_pci_submit_sq_cmd(nvmeq, cmd, TRUE);

    blockdriver_async_sleep();

    if (buffer && bufflen) {
        nvme_unmap_data(&iod);
    }

    if (result) *result = cqe_result[tid];

    return cqe_status[tid];
}

static int nvme_features(u8 op, unsigned int fid, unsigned int dword11,
                         u32* result)
{
    struct nvme_command c;
    union nvme_result res = {0};
    int retval;

    memset(&c, 0, sizeof(c));
    c.features.opcode = op;
    c.features.fid = cpu_to_le32(fid);
    c.features.dword11 = cpu_to_le32(dword11);

    retval = nvme_submit_cmd(nvme_queues, &c, NULL, 0, &res);

    if (retval >= 0 && result) *result = le32_to_cpu(res.u32);
    return retval;
}

static int nvme_set_features(unsigned int fid, unsigned int dword11,
                             u32* result)
{
    return nvme_features(nvme_admin_set_features, fid, dword11, result);
}

static int nvme_set_queue_count(int* count)
{
    u32 result;
    u32 q_count = (*count - 1) | ((*count - 1) << 16);
    int nioqs;
    int status;

    status = nvme_set_features(NVME_FEAT_NUM_QUEUES, q_count, &result);

    if (status < 0)
        return status;
    else if (status > 0)
        *count = 0;
    else {
        nioqs = min(result & 0xffff, result >> 16) + 1;
        *count = min(*count, nioqs);
    }

    return 0;
}

static int nvme_setup_rw(struct nvme_ns* ns, struct nvme_command* cmd,
                         int do_write, loff_t pos, size_t size)
{
    u16 control = 0;
    u32 dsmgmt = 0;

    memset(cmd, 0, sizeof(*cmd));
    cmd->rw.opcode = (do_write ? nvme_cmd_write : nvme_cmd_read);
    cmd->rw.nsid = cpu_to_le32(ns->nsid);
    cmd->rw.slba = cpu_to_le64(pos >> ns->lba_shift);
    cmd->rw.length = cpu_to_le16((size >> ns->lba_shift) - 1);

    cmd->rw.control = cpu_to_le16(control);
    cmd->rw.dsmgmt = cpu_to_le32(dsmgmt);

    return 0;
}

static int nvme_status_error(u32 status)
{
    switch (status & 0x7ff) {
    case NVME_SC_CAP_EXCEEDED:
        return ENOSPC;
    default:
        return EIO;
    }
}

static ssize_t nvme_pci_rdwt(dev_t minor, int do_write, loff_t pos,
                             endpoint_t endpoint, const struct iovec_grant* iov,
                             size_t count)
{
    int i;
    struct nvme_ns* ns;
    struct part_info* part;
    struct nvme_queue* nvmeq;
    struct nvme_command cmd;
    u64 part_end;
    struct iovec_grant virs[NR_IOREQS];
    struct umap_phys phys[NR_IOREQS];
    blockdriver_worker_id_t tid;
    size_t size, tmp_size;
    struct nvme_iod iod;
    u32 status;
    int retval;

    if (count > NR_IOREQS) {
        return -EINVAL;
    }

    tid = blockdriver_async_worker_id();

    ns = nvme_find_get_ns(NSID_OF_DEV(minor));
    part = nvme_pci_part(minor);

    if (!ns || !part) {
        return -ENXIO;
    }

    nvmeq = &nvme_queues[1 + (tid % (online_queues - 1))];

    pos += part->base;

    if (pos & ((1 << ns->lba_shift) - 1)) {
        return -EINVAL;
    }

    part_end = part->base + part->size;

    if (pos >= part_end) {
        return 0;
    }

    size = 0;
    for (i = 0; i < count; i++) {
        size += iov[i].iov_len;
    }

    if (pos + size >= part_end) {
        size = part_end - pos;
        tmp_size = 0;
        count = 0;
    } else {
        tmp_size = size;
    }

    while (tmp_size < size) {
        tmp_size += iov[count++].iov_len;
    }

    for (i = 0; i < count; i++) {
        if (endpoint == SELF)
            virs[i].iov_addr = iov[i].iov_addr;
        else
            virs[i].iov_grant = iov[i].iov_grant;

        virs[i].iov_len = iov[i].iov_len;
    }

    if (tmp_size > size) {
        virs[count - 1].iov_len -= (tmp_size - size);
    }

    if (size & ((1 << ns->lba_shift) - 1)) {
        return -EINVAL;
    }

    retval = nvme_setup_rw(ns, &cmd, do_write, pos, size);
    if (retval) return retval;

    retval = nvme_fill_buffers(virs, phys, count, endpoint);
    if (retval) return retval;

    retval = nvme_map_data(&cmd, phys, count, size, &iod);
    if (retval) return retval;

    nvme_pci_submit_sq_cmd(nvmeq, &cmd, TRUE);

    blockdriver_async_sleep();

    status = cqe_status[tid];

    nvme_unmap_data(&iod);

    if ((status & 0x7ff) == NVME_SC_SUCCESS) return size;

    return -nvme_status_error(status);
}

static int nvme_alloc_queue(int qid, int depth)
{
    struct nvme_queue* nvmeq = &nvme_queues[qid];

    if (qid < queue_count) return 0;

    nvmeq->q_depth = depth;
    nvmeq->sqes = qid ? NVME_NVM_IOSQES : NVME_ADM_SQES;

    nvmeq->cqes =
        mmap(NULL, CQ_SIZE(nvmeq), PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (nvmeq->cqes == MAP_FAILED) goto free;

    if (umap(SELF, UMT_VADDR, (vir_bytes)nvmeq->cqes, CQ_SIZE(nvmeq),
             &nvmeq->cq_phys) != 0) {
        panic("%s: umap failed", name);
    }

    nvmeq->sq_cmds =
        mmap(NULL, SQ_SIZE(nvmeq), PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (nvmeq->sq_cmds == MAP_FAILED) goto unmap_cqes;

    if (umap(SELF, UMT_VADDR, (vir_bytes)nvmeq->sq_cmds, SQ_SIZE(nvmeq),
             &nvmeq->sq_phys) != 0) {
        panic("%s: umap failed", name);
    }

    nvmeq->cq_head = 0;
    nvmeq->q_db = &nvme_dbs[qid * 2 * db_stride];
    nvmeq->qid = qid;
    nvmeq->cq_phase = 1;
    queue_count++;

    return 0;

unmap_cqes:
    munmap((void*)nvmeq->cqes, CQ_SIZE(nvmeq));
free:
    return ENOMEM;
}

static void nvme_init_queue(int qid)
{
    struct nvme_queue* nvmeq = &nvme_queues[qid];

    nvmeq->cq_head = 0;
    nvmeq->sq_tail = 0;
    nvmeq->last_sq_tail = 0;
    nvmeq->q_db = &nvme_dbs[qid * 2 * db_stride];
    nvmeq->cq_phase = 1;
    memset((void*)nvmeq->cqes, 0, CQ_SIZE(nvmeq));
    online_queues++;
}

static int nvme_pci_read_cap(void)
{
    if (nvme_pci_read32(nvme_bar + NVME_REG_CSTS) == -1) {
        return ENODEV;
    }

    ctrl_cap = nvme_pci_read64(nvme_bar + NVME_REG_CAP);

    queue_depth = min(NVME_CAP_MQES(ctrl_cap) + 1, IO_QUEUE_DEPTH);
    db_stride = 1 << NVME_CAP_STRIDE(ctrl_cap);

    return 0;
}

static int nvme_wait_ready(int enabled)
{
    u32 csts, bit = enabled ? NVME_CSTS_RDY : 0;

    while (TRUE) {
        csts = nvme_pci_read32(nvme_bar + NVME_REG_CSTS);
        if (csts == ~0) return ENODEV;

        if ((csts & NVME_CSTS_RDY) == bit) break;
    }

    return 0;
}

static int nvme_disable_controller(void)
{
    ctrl_config &= ~NVME_CC_SHN_MASK;
    ctrl_config &= ~NVME_CC_ENABLE;

    nvme_pci_write32(nvme_bar + NVME_REG_CC, ctrl_config);

    return nvme_wait_ready(FALSE);
}

static int nvme_enable_controller(void)
{
    unsigned dev_page_min, page_shift = 12;

    ctrl_cap = nvme_pci_read64(nvme_bar + NVME_REG_CAP);
    dev_page_min = NVME_CAP_MPSMIN(ctrl_cap) + 12;

    if (page_shift < dev_page_min) return ENODEV;

    ctrl_page_size = 1 << page_shift;

    ctrl_config = NVME_CC_CSS_NVM;
    ctrl_config |= (page_shift - 12) << NVME_CC_MPS_SHIFT;
    ctrl_config |= NVME_CC_AMS_RR | NVME_CC_SHN_NONE;
    ctrl_config |= NVME_CC_IOSQES | NVME_CC_IOCQES;
    ctrl_config |= NVME_CC_ENABLE;

    nvme_pci_write32(nvme_bar + NVME_REG_CC, ctrl_config);

    return nvme_wait_ready(TRUE);
}

static int nvme_pci_setup_admin_queue(void)
{
    int retval;
    u32 aqa;
    struct nvme_queue* adminq;

    retval = nvme_pci_remap_bar(db_bar_size(0));
    if (retval) return retval;

    retval = nvme_disable_controller();
    if (retval) return retval;

    retval = nvme_alloc_queue(0, NVME_AQ_DEPTH);
    if (retval) return retval;

    adminq = &nvme_queues[0];

    aqa = adminq->q_depth - 1;
    aqa |= aqa << 16;

    nvme_pci_write32(nvme_bar + NVME_REG_AQA, aqa);
    nvme_pci_write64(nvme_bar + NVME_REG_ASQ, (uint64_t)adminq->sq_phys);
    nvme_pci_write64(nvme_bar + NVME_REG_ACQ, (uint64_t)adminq->cq_phys);

    retval = nvme_enable_controller();
    if (retval) return retval;

    nvme_init_queue(0);

    return 0;
}

static int nvme_create_cq(struct nvme_queue* nvmeq, u16 qid, u16 vector)
{
    struct nvme_command c;
    struct nvme_queue* adminq = &nvme_queues[0];
    int flags = NVME_QUEUE_PHYS_CONTIG | NVME_CQ_IRQ_ENABLED;

    memset(&c, 0, sizeof(c));
    c.create_cq.opcode = nvme_admin_create_cq;
    c.create_cq.prp1 = cpu_to_le64((u64)nvmeq->cq_phys);
    c.create_cq.cqid = cpu_to_le16(qid);
    c.create_cq.qsize = cpu_to_le16(nvmeq->q_depth - 1);
    c.create_cq.cq_flags = cpu_to_le16(flags);
    c.create_cq.irq_vector = cpu_to_le16(vector);

    return nvme_submit_cmd(adminq, &c, NULL, 0, NULL);
}

static int nvme_create_sq(struct nvme_queue* nvmeq, u16 qid)
{
    struct nvme_command c;
    struct nvme_queue* adminq = &nvme_queues[0];
    int flags = NVME_QUEUE_PHYS_CONTIG;

    memset(&c, 0, sizeof(c));
    c.create_sq.opcode = nvme_admin_create_sq;
    c.create_sq.prp1 = cpu_to_le64((u64)nvmeq->sq_phys);
    c.create_sq.sqid = cpu_to_le16(qid);
    c.create_sq.qsize = cpu_to_le16(nvmeq->q_depth - 1);
    c.create_sq.sq_flags = cpu_to_le16(flags);
    c.create_sq.cqid = cpu_to_le16(qid);

    return nvme_submit_cmd(adminq, &c, NULL, 0, NULL);
}

static int nvme_delete_queue(u8 opcode, u16 qid)
{
    struct nvme_command c;
    struct nvme_queue* adminq = &nvme_queues[0];

    memset(&c, 0, sizeof(c));
    c.delete_queue.opcode = opcode;
    c.delete_queue.qid = cpu_to_le16(qid);

    return nvme_submit_cmd(adminq, &c, NULL, 0, NULL);
}

static inline int nvme_delete_cq(u16 qid)
{
    return nvme_delete_queue(nvme_admin_delete_cq, qid);
}

static inline int nvme_delete_sq(u16 qid)
{
    return nvme_delete_queue(nvme_admin_delete_sq, qid);
}

static int nvme_create_queue(struct nvme_queue* nvmeq, int qid)
{
    u16 vector = 0;
    int retval;

    retval = nvme_create_cq(nvmeq, qid, vector);
    if (retval) return retval;

    retval = nvme_create_sq(nvmeq, qid);
    if (retval < 0) return retval;
    if (retval) goto release_cq;

    nvme_init_queue(qid);

    return 0;

release_cq:
    nvme_delete_cq(qid);
    return retval;
}

static int nvme_create_io_queues(int nr_io_queues)
{
    int i;
    int retval = 0;
    int max_qs;

    for (i = queue_count; i <= nr_io_queues; i++) {
        if (nvme_alloc_queue(i, queue_depth)) {
            retval = -ENOMEM;
            break;
        }
    }

    max_qs = min(queue_count - 1, nr_io_queues);

    for (i = online_queues; i <= max_qs; i++) {
        retval = nvme_create_queue(&nvme_queues[i], i);
        if (retval) break;
    }

    return retval >= 0 ? 0 : -retval;
}

static int nvme_setup_io_queues(void)
{
    int nr_io_queues;
    unsigned long size;
    struct nvme_queue* adminq;
    int retval;

    nr_io_queues = max_io_queues();

    retval = nvme_set_queue_count(&nr_io_queues);
    if (retval) return -retval;

    if (nr_io_queues == 0) return 0;

    adminq = &nvme_queues[0];

    do {
        size = db_bar_size(nr_io_queues);
        retval = nvme_pci_remap_bar(size);
        if (!retval) break;
        if (!--nr_io_queues) return ENOMEM;
    } while (TRUE);

    adminq->q_db = nvme_dbs;

    retval = nvme_create_io_queues(nr_io_queues);
    if (retval) return retval;

    return 0;
}

static inline int nvme_cqe_pending(struct nvme_queue* nvmeq)
{
    return (le16_to_cpu(nvmeq->cqes[nvmeq->cq_head].status) & 1) ==
           nvmeq->cq_phase;
}

static inline void nvme_ring_cq_doorbell(struct nvme_queue* nvmeq)
{
    nvme_pci_write32(nvmeq->q_db + db_stride, nvmeq->cq_head);
}

static inline void nvme_handle_cqe(struct nvme_queue* nvmeq, u16 idx)
{
    volatile struct nvme_completion* cqe = &nvmeq->cqes[idx];
    blockdriver_worker_id_t tid;

    tid = (blockdriver_worker_id_t)cqe->command_id;

    cqe_status[tid] = cqe->status >> 1;
    cqe_result[tid] = cqe->result;

    blockdriver_async_wakeup(tid);
}

static inline void nvme_update_cq_head(struct nvme_queue* nvmeq)
{
    u16 tmp = nvmeq->cq_head + 1;

    if (tmp == nvmeq->q_depth) {
        nvmeq->cq_head = 0;
        nvmeq->cq_phase ^= 1;
    } else {
        nvmeq->cq_head = tmp;
    }
}

static int nvme_process_cq(struct nvme_queue* nvmeq)
{
    int found = 0;

    while (nvme_cqe_pending(nvmeq)) {
        found++;
        nvme_handle_cqe(nvmeq, nvmeq->cq_head);
        nvme_update_cq_head(nvmeq);
    }

    if (found) nvme_ring_cq_doorbell(nvmeq);

    return found;
}

static void nvme_pci_intr(unsigned mask)
{
    int i;
    for (i = 0; i < queue_count; i++)
        nvme_process_cq(&nvme_queues[i]);

    irq_enable(&pin_irq_hook);
}

static int nvme_identify_controller(struct nvme_id_ctrl** id)
{
    struct nvme_command c;
    struct nvme_queue* adminq = &nvme_queues[0];
    int retval;

    memset(&c, 0, sizeof(c));
    c.identify.opcode = nvme_admin_identify;
    c.identify.cns = NVME_ID_CNS_CTRL;

    *id = mmap(NULL, sizeof(struct nvme_id_ctrl), PROT_READ | PROT_WRITE,
               MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (*id == MAP_FAILED) return -ENOMEM;

    retval =
        nvme_submit_cmd(adminq, &c, *id, sizeof(struct nvme_id_ctrl), NULL);
    if (retval) munmap(*id, sizeof(struct nvme_id_ctrl));
    return retval;
}

static int nvme_identify_ns_list(unsigned int nsid, __le32* ns_list)
{
    struct nvme_command c;
    struct nvme_queue* adminq = &nvme_queues[0];

    memset(&c, 0, sizeof(c));
    c.identify.opcode = nvme_admin_identify;
    c.identify.cns = NVME_ID_CNS_NS_ACTIVE_LIST;
    c.identify.nsid = cpu_to_le32(nsid);

    return nvme_submit_cmd(adminq, &c, ns_list, NVME_IDENTIFY_DATA_SIZE, NULL);
}

static int nvme_identify_ns(unsigned int nsid, struct nvme_id_ns** id)
{
    struct nvme_command c;
    struct nvme_queue* adminq = &nvme_queues[0];
    int retval;

    memset(&c, 0, sizeof(c));
    c.identify.opcode = nvme_admin_identify;
    c.identify.cns = NVME_ID_CNS_NS;
    c.identify.nsid = cpu_to_le32(nsid);

    *id = mmap(NULL, sizeof(struct nvme_id_ns), PROT_READ | PROT_WRITE,
               MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (*id == MAP_FAILED) return -ENOMEM;

    retval =
        nvme_submit_cmd(adminq, &c, *id, sizeof(struct nvme_id_ctrl), NULL);
    if (retval) munmap(*id, sizeof(struct nvme_id_ns));
    return retval;
}

static int nvme_init_identify(void)
{
    struct nvme_id_ctrl* id;
    int page_shift;
    u32 max_sectors;
    int retval;

    page_shift = NVME_CAP_MPSMIN(ctrl_cap) + 12;

    retval = nvme_identify_controller(&id);
    if (retval) return EIO;

    if (id->mdts)
        max_sectors = 1 << (id->mdts + page_shift - 9);
    else
        max_sectors = UINT32_MAX;

    if (max_hw_sectors)
        max_hw_sectors = min(max_hw_sectors, max_sectors);
    else
        max_hw_sectors = max_sectors;

    munmap(id, sizeof(*id));

    return 0;
}

static int nvme_reset(void)
{
    int retval;

    retval = nvme_pci_read_cap();
    if (retval) return retval;

    retval = nvme_pci_setup_admin_queue();
    if (retval) return retval;

    retval = nvme_init_identify();
    if (retval) return retval;

    retval = nvme_setup_io_queues();
    if (retval) return retval;

    return 0;
}

static void nvme_alloc_ns(unsigned int nsid)
{
    struct nvme_ns* ns;
    struct nvme_id_ns* id;
    int retval;

    ns = malloc(sizeof(*ns));
    if (!ns) return;

    memset(ns, 0, sizeof(*ns));
    kref_init(&ns->kref);
    ns->nsid = nsid;
    ns->lba_shift = 9;

    retval = nvme_identify_ns(nsid, &id);
    if (retval) goto free_ns;

    ns->lba_shift = id->lbaf[id->flbas & NVME_NS_FLBAS_LBA_MASK].ds;
    if (ns->lba_shift == 0) ns->lba_shift = 9;

    ns->capacity = nvme_lba_to_sect(ns, le64_to_cpu(id->nsze));

    list_add_tail(&ns->list, &namespaces);

    munmap(id, sizeof(*id));
    return;

free_ns:
    free(ns);
}

static void nvme_validate_ns(unsigned int nsid)
{
    struct nvme_ns* ns;

    ns = nvme_find_get_ns(nsid);
    if (ns)
        nvme_put_ns(ns);
    else
        nvme_alloc_ns(nsid);
}

static int nvme_scan_ns_list(unsigned int nn)
{
    __le32* ns_list;
    unsigned int num_lists = (nn + 1023) >> 10;
    unsigned int prev = 0;
    u32 nsid;
    int i, j, retval;

    ns_list =
        mmap(NULL, NVME_IDENTIFY_DATA_SIZE, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (ns_list == MAP_FAILED) return -ENOMEM;

    for (i = 0; i < num_lists; i++) {
        struct nvme_ns* ns;

        retval = nvme_identify_ns_list(prev, ns_list);
        if (retval) goto out;

        for (j = 0; j < min(1024, nn); j++) {
            nsid = le32_to_cpu(ns_list[j]);
            if (!nsid) goto out;

            nvme_validate_ns(nsid);

            while (++prev < nsid) {
                if ((ns = nvme_find_get_ns(prev))) {
                    nvme_remove_ns(ns);
                    nvme_put_ns(ns);
                }
            }
        }
        nn -= j;
    }

out:
    munmap(ns_list, NVME_IDENTIFY_DATA_SIZE);
    return retval;
}

static int nvme_scan(void)
{
    struct nvme_id_ctrl* id;
    unsigned int nn;
    int retval;

    if (nvme_identify_controller(&id)) return EIO;

    nn = le32_to_cpu(id->nn);

    retval = nvme_scan_ns_list(nn);
    if (retval < 0)
        retval = -retval;
    else if (retval > 0)
        retval = EIO;

    munmap(id, sizeof(*id));
    return retval;
}

static int nvme_pci_probe(int instance)
{
    int devind;
    u16 vid, did;
    device_id_t dev_id;
    unsigned long base;
    size_t size;
    struct device_info devinf;
    int ioflag;
    int irq;
    int retval;

    retval = pci_first_dev(&devind, &vid, &did, &dev_id);
    while (retval == 0) {
        if (instance == 0) break;
        instance--;

        retval = pci_next_dev(&devind, &vid, &did, &dev_id);
    }

    if (retval || instance) return ENXIO;

    nvme_queues = calloc(max_queue_count(), sizeof(struct nvme_queue));
    if (!nvme_queues) return ENOMEM;

    irq = pci_attr_r8(devind, PCI_ILR);
    pin_irq_hook = 0;

    retval = irq_setpolicy(irq, 0, &pin_irq_hook);
    if (retval) goto free;

    retval = irq_enable(&pin_irq_hook);
    if (retval) {
        irq_rmpolicy(&pin_irq_hook);
        goto free;
    }

    retval = pci_get_bar(devind, PCI_BAR, &base, &size, &ioflag);
    if (retval) goto free;

    nvme_bar_base = base;

    retval = nvme_pci_remap_bar(NVME_REG_DBS + 4096);
    if (retval) goto free;

    memset(&devinf, 0, sizeof(devinf));
    snprintf(devinf.name, sizeof(devinf.name), "nvme%d", instance);
    devinf.bus = NO_BUS_ID;
    devinf.class = nvme_class;
    devinf.parent = dev_id;
    devinf.devt = NO_DEV;

    retval = dm_device_register(&devinf, &nvme_dev_id);
    if (retval) goto free;

    return 0;

free:
    free(nvme_queues);
    nvme_queues = NULL;

    return retval;
}

static int nvme_pci_init(void)
{
    int retval;

    instance = 0;

    printl("%s: NVMe block driver is running\n", name);

    INIT_LIST_HEAD(&namespaces);

    retval = dm_class_register("nvme", &nvme_class);
    if (retval) return retval;

    retval = nvme_pci_probe(instance);
    if (retval) return retval;

    return 0;
}

static void print_ns_part_info(struct nvme_ns* ns)
{
    int i;
    printl("%s: Namespace %d:\n", name, ns->nsid);
    for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) {
        if (ns->primary[i].size == 0) continue;

        u32 base_sect = ns->primary[i].base >> SECTOR_SIZE_SHIFT;
        u32 nr_sects = ns->primary[i].size >> SECTOR_SIZE_SHIFT;

        printl(
            "%s: %spartition %d: base %u(0x%x), size %u(0x%x) (in sectors)\n",
            name, i == 0 ? "  " : "     ", i, base_sect, base_sect, nr_sects,
            nr_sects);
    }
    for (i = 0; i < NR_SUB_PER_DRIVE; i++) {
        if (ns->logical[i].size == 0) continue;

        u32 base_sect = ns->logical[i].base >> SECTOR_SIZE_SHIFT;
        u32 nr_sects = ns->logical[i].size >> SECTOR_SIZE_SHIFT;

        printl("%s:              "
               "%d: base %d(0x%x), size %d(0x%x) (in sectors)\n",
               name, i, base_sect, base_sect, nr_sects, nr_sects);
    }
}

static void nvme_register_ns(struct nvme_ns* ns)
{
    int i;
    u32 nsid = ns->nsid;
    struct device_info devinf;
    dev_t devt;
    device_id_t dev_id;

    /* register the device */
    memset(&devinf, 0, sizeof(devinf));
    devinf.bus = NO_BUS_ID;
    devinf.class = nvme_class;
    devinf.parent = nvme_dev_id;
    devinf.type = DT_BLOCKDEV;

    for (i = 0; i < NR_PART_PER_DRIVE + 1; i++) {
        if (ns->primary[i].size == 0) continue;

        devt = MAKE_DEV(DEV_NVME, nsid * NR_SUB_PER_DRIVE + i);
        dm_bdev_add(devt);

        snprintf(devinf.name, sizeof(devinf.name), "nvme%dn%dp%d", instance,
                 nsid, i);
        devinf.devt = devt;
        dm_device_register(&devinf, &dev_id);
    }

    for (i = 0; i < NR_SUB_PER_DRIVE; i++) {
        if (ns->logical[i].size == 0) continue;

        devt =
            MAKE_DEV(DEV_HD, nsid * NR_SUB_PER_DRIVE + NR_PRIM_PER_DRIVE + i);
        dm_bdev_add(devt);

        snprintf(devinf.name, sizeof(devinf.name), "nvme%dn%dp%d", instance,
                 nsid, NR_PRIM_PER_DRIVE + i);
        devinf.devt = devt;
        dm_device_register(&devinf, &dev_id);
    }
}

static void nvme_pci_post_init(void)
{
    struct nvme_ns* ns;

    nvme_reset();
    nvme_scan();

    list_for_each_entry(ns, &namespaces, list)
    {
        ns->primary[0].size = ns->capacity << SECTOR_SIZE_SHIFT;
        partition(&nvme_pci_driver, ns->nsid * NR_SUB_PER_DRIVE, P_PRIMARY);
        print_ns_part_info(ns);
        nvme_register_ns(ns);
    }
}

int main()
{
    serv_register_init_fresh_callback(nvme_pci_init);
    serv_init();

    blockdriver_async_task(&nvme_pci_driver, MAX_THREADS, nvme_pci_post_init);

    return 0;
}
