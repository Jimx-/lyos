#include <lyos/types.h>
#include <lyos/ipc.h>
#include <assert.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <lyos/sysutils.h>
#include <sys/mman.h>

#include <libblockdriver/libblockdriver.h>

/*****************************************************************************
 *                                get_part_table
 *****************************************************************************/
/**
 * <Ring 1> Get a partition table of a drive.
 *
 * @param drive   Drive nr (0 for the 1st disk, 1 for the 2nd, ...)
 * @param sect_nr The sector at which the partition table is located.
 * @param entry   Ptr to part_ent struct.
 *****************************************************************************/
static int get_part_table(struct blockdriver* bd, int device,
                          unsigned long sect_nr, struct part_ent* entry,
                          u8* tmp_buf)
{
    int retval;

    struct iovec_grant iov;
    iov.iov_addr = (vir_bytes)tmp_buf;
    iov.iov_len = CD_SECTOR_SIZE;
    retval = bd->bdr_readwrite(device, FALSE, sect_nr << SECTOR_SIZE_SHIFT,
                               SELF, &iov, 1);

    if (retval != CD_SECTOR_SIZE) {
        return 0;
    }

    memcpy(entry, tmp_buf + PARTITION_TABLE_OFFSET,
           sizeof(struct part_ent) * NR_PART_PER_DRIVE);

    return 1;
}

static void parse_part_table(struct blockdriver* bd, int device, int style,
                             u8* tmp_buf)

{
    int i;
    struct part_ent part_tbl[NR_SUB_PER_DRIVE];

    if (style == P_PRIMARY) {
        get_part_table(bd, device, 0, part_tbl, tmp_buf);

        switch (style) {
        case P_PRIMARY:
            device += 1;
            break;
        }

        struct part_info* part = bd->bdr_part(device);
        if (part == NULL) return;

        int nr_prim_parts = 0;
        for (i = 0; i < NR_PART_PER_DRIVE; i++, part++) { /* 0~3 */
            if (part_tbl[i].sys_id == NO_PART) continue;

            nr_prim_parts++;
            int dev_nr = i + 1; /* 1~4 */
            part->base = part_tbl[i].start_sect << SECTOR_SIZE_SHIFT;
            part->size = part_tbl[i].nr_sects << SECTOR_SIZE_SHIFT;

            if (part_tbl[i].sys_id == EXT_PART) /* extended */
                partition(bd, device + dev_nr, P_EXTENDED);
        }
        assert(nr_prim_parts != 0);
    } else if (style == P_EXTENDED) {
        struct part_info* part = bd->bdr_part(device);
        if (part == NULL) return;

        int drive = device - (device % NR_SUB_PER_DRIVE);
        device = drive + NR_PRIM_PER_DRIVE + 1;
        part = bd->bdr_part(device);

        while (part->size != 0)
            part++;

        u64 extbase = part->base;
        u64 s = extbase;

        for (i = 0; i < NR_SUB_PER_PART; i++, part++) {
            get_part_table(bd, device, s, part_tbl, tmp_buf);

            part->base = (extbase + part_tbl[0].start_sect)
                         << SECTOR_SIZE_SHIFT;
            part->size = part_tbl[0].nr_sects << SECTOR_SIZE_SHIFT;

            s = extbase + part_tbl[1].start_sect;

            /* no more logical partitions
               in this extended partition */
            if (part_tbl[1].sys_id == NO_PART) break;
        }
    } else {
        assert(0);
    }
}

/*****************************************************************************
 *                                partition
 *****************************************************************************/
/**
 * <Ring 1> This routine is called when a device is opened. It reads the
 * partition table(s) and fills the hd_info struct.
 *
 * @param device Device nr.
 * @param style  P_PRIMARY or P_EXTENDED.
 *****************************************************************************/
void partition(struct blockdriver* bd, int device, int style)
{
    u8* tmp_buf =
        mmap(NULL, CD_SECTOR_SIZE, PROT_READ | PROT_WRITE,
             MAP_POPULATE | MAP_ANONYMOUS | MAP_CONTIG | MAP_PRIVATE, -1, 0);
    if (tmp_buf == MAP_FAILED) {
        panic("partition: unable to allocate temporary buffer");
    }

    parse_part_table(bd, device, style, tmp_buf);

    munmap(tmp_buf, CD_SECTOR_SIZE);
}
