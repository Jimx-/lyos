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

#ifndef	_HPET_H_
#define	_HPET_H_

#define HPET_ID                 0x000
#define HPET_PERIOD             0x004
#define HPET_CFG                0x010
#define HPET_STATUS             0x020
#define HPET_COUNTER            0x0f0

#define HPET_CFG_ENABLE         0x001
#define HPET_CFG_LEGACY         0x002

PUBLIC int init_hpet();
PUBLIC u8 is_hpet_enabled();
PUBLIC u32 hpet_read(u32 a);
PUBLIC u32 hpet_write(u32 a, u32 v);

#endif
