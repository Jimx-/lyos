/*  This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */
    
#include "lyos/type.h"
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/dmi.h"

#define ROM_BASE (0xF0000 + KERNEL_VMA)
#define ROM_SIZE 0x10000

static int dmi_available = 0;
static int dmi_ver = 0;
static u16 dmi_num = 0, dmi_len = 0;
static u32 dmi_base = 0;

static char * dmi_info[DMI_MAX_STRINGS];

PRIVATE int dmi_present(const char * buf);
PRIVATE char * get_dmi_string(const struct dmi_header * dh, int index);
PRIVATE void dmi_decode(const struct dmi_header * dh);
PRIVATE void dmi_table(char * buf, int len, int num);
PRIVATE int dmi_checksum(const char *buf, u8 len);
PRIVATE void dmi_save_info(const struct dmi_header * dh, int slot, int index);

PUBLIC void dmi_init()
{
    char buf[32];

    memset(buf, 0, 16);

    int p;
    for (p = ROM_BASE; p < ROM_BASE + ROM_SIZE; p += 16) {
        memcpy(buf + 16, (char*)p, 16);
        if (dmi_present(buf)) {
            dmi_available = 1;
            goto ok;
        }
        memcpy(buf, buf + 16, 16);
    }

    printl("DMI not present.\n");
ok:
    return;
}

PRIVATE int dmi_present(const char * buf)
{
    int ver = 0;
    if (memcmp(buf, "_SM_", 4) == 0 && dmi_checksum(buf, buf[5])) {
        ver = (buf[6] << 8) + buf[7];
    }

    buf += 16;

    if (memcmp(buf, "_DMI_", 5) == 0 && dmi_checksum(buf, 15)) {
        dmi_num = (buf[13] << 8) | buf[12];
        dmi_len = (buf[7] << 8) | buf[6];
        dmi_base = ((buf[11] & 0xFF) << 24) | ((buf[10] & 0xFF) << 16) |
                            ((buf[9] & 0xFF) << 8) | (buf[8] & 0xFF);

        if (ver) {
            dmi_ver = ver;
            printl("SMBIOS %d.%d present.\n", dmi_ver >> 8, dmi_ver & 0xFF);
        } else {
            dmi_ver = buf[14];
            if (dmi_ver) {
                printl("DMI %d.%d present.\n", dmi_ver >> 8, dmi_ver & 0xFF);
            } else {
                printl("DMI present.\n");
            }
        }

        dmi_table((char*)(dmi_base + KERNEL_VMA), dmi_len, dmi_num);
        

        return 1;
    }

    return 0;
}

PRIVATE int dmi_checksum(const char *buf, u8 len)
{
    u8 sum = 0;
    int a;

    for (a = 0; a < len; a++)
        sum += buf[a];

    return sum == 0;
}


PRIVATE void dmi_table(char * buf, int len, int num)
{
    int i = 0;
    char * data = buf;

    while ((i < num) && (data - buf + sizeof(struct dmi_header)) <= len) {
        struct dmi_header * dh = (struct dmi_header *)data;
        data += dh->length;
        // skip strings
        while ((data - buf < len - 1) && (data[0] || data[1]))
            data++;
        if (data - buf < len - 1)
            dmi_decode(dh);
        data += 2;
        i++;
    }
}

PRIVATE char * get_dmi_string(const struct dmi_header * dh, int index)
{
    char * s = (char *)dh + dh->length;

    if (index) {
        index--;
        while (index > 0 && *s) {
            s += strlen(s) + 1;
            index--;
        }

        if (*s) {
            return s;
        }
    }

    return NULL;
}

PRIVATE void dmi_save_info(const struct dmi_header * dh, int slot, int index)
{
    char * s = get_dmi_string(dh, index);

    dmi_info[slot] = s;
}

PUBLIC char * dmi_get_info(int slot)
{
    return dmi_info[slot];
}

PRIVATE void dmi_decode(const struct dmi_header * dh)
{
    //int i = 0;
    switch (dh->type) {
        case 0: 
            /* BIOS Information */
            dmi_save_info(dh, DMI_BIOS_VENDOR, 1);
            dmi_save_info(dh, DMI_BIOS_VERSION, 2);
            dmi_save_info(dh, DMI_BIOS_DATE, 3);
            break;
        case 1:         
            //for (i = 0; i < 9; i++) printl("%s\n", get_dmi_string(dh, i));
            /* System Information */
            dmi_save_info(dh, DMI_SYS_VENDOR, 1);
            dmi_save_info(dh, DMI_PRODUCT_NAME, 2);
            dmi_save_info(dh, DMI_PRODUCT_VERSION, 3);
            dmi_save_info(dh, DMI_PRODUCT_SERIAL, 4);
            break;
    }
}
