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

#ifndef _DMI_H_
#define _DMI_H_

#define DMI_BIOS_VENDOR     0
#define DMI_BIOS_VERSION    1
#define DMI_BIOS_DATE       2
#define DMI_SYS_VENDOR      3
#define DMI_PRODUCT_NAME    4
#define DMI_PRODUCT_VERSION 5
#define DMI_PRODUCT_SERIAL  6

#define DMI_MAX_STRINGS     7

struct dmi_header {
    u8  type;
    u8  length;
    u16 handle;
};

PUBLIC void dmi_init();
PUBLIC char * dmi_get_info(int slot);

#endif
