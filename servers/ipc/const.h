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

#ifndef _IPC_CONST_H_
#define _IPC_CONST_H_

#define IPCID_TO_IX(id) ((id)&0xffff)
#define IPCID_TO_SEQ(id) (((id) >> 16) & 0xffff)

#define IXSEQ_TO_IPCID(ix, seq) (((seq) << 16) | ((ix)&0xffff))

#endif
