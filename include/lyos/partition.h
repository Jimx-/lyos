#ifndef _PARTITION_H_
#define _PARTITION_H_

/**
 * @struct part_ent
 * @brief  Partition Entry struct.
 *
 * <b>Master Boot Record (MBR):</b>
 *   Located at offset 0x1BE in the 1st sector of a disk. MBR contains
 *   four 16-byte partition entries. Should end with 55h & AAh.
 *
 * <b>partitions in MBR:</b>
 *   A PC hard disk can contain either as many as four primary partitions,
 *   or 1-3 primaries and a single extended partition. Each of these
 *   partitions are described by a 16-byte entry in the Partition Table
 *   which is located in the Master Boot Record.
 *
 * <b>extented partition:</b>
 *   It is essentially a link list with many tricks. See
 *   http://en.wikipedia.org/wiki/Extended_boot_record for details.
 */
struct part_ent {
    u8 boot_ind; /**
                  * boot indicator
                  *   Bit 7 is the active partition flag,
                  *   bits 6-0 are zero (when not zero this
                  *   byte is also the drive number of the
                  *   drive to boot so the active partition
                  *   is always found on drive 80H, the first
                  *   hard disk).
                  */

    u8 start_head; /**
                    * Starting Head
                    */

    u8 start_sector; /**
                      * Starting Sector.
                      *   Only bits 0-5 are used. Bits 6-7 are
                      *   the upper two bits for the Starting
                      *   Cylinder field.
                      */

    u8 start_cyl; /**
                   * Starting Cylinder.
                   *   This field contains the lower 8 bits
                   *   of the cylinder value. Starting cylinder
                   *   is thus a 10-bit number, with a maximum
                   *   value of 1023.
                   */

    u8 sys_id; /**
                * System ID
                * e.g.
                *   01: FAT12
                *   81: MINIX
                *   83: Linux
                */

    u8 end_head; /**
                  * Ending Head
                  */

    u8 end_sector; /**
                    * Ending Sector.
                    *   Only bits 0-5 are used. Bits 6-7 are
                    *   the upper two bits for the Ending
                    *    Cylinder field.
                    */

    u8 end_cyl; /**
                 * Ending Cylinder.
                 *   This field contains the lower 8 bits
                 *   of the cylinder value. Ending cylinder
                 *   is thus a 10-bit number, with a maximum
                 *   value of 1023.
                 */

    u32 start_sect; /**
                     * starting sector counting from
                     * 0 / Relative Sector. / start in LBA
                     */

    u32 nr_sects; /**
                   * nr of sectors in partition
                   */

} PARTITION_ENTRY;

#define NR_PART_PER_DRIVE 4 /* number of entries in a partition table */
#define PARTITION_TABLE_OFFSET \
    0x1BE /* offset of partition table in the boot sector */

#define NO_PART 0x00  /* unused entry */
#define EXT_PART 0x05 /* extended partition */

#endif
