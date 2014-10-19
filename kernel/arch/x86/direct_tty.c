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
#include "stdarg.h"
#include "unistd.h"
#include "assert.h"
#include "protect.h"
#include "lyos/const.h"
#include "lyos/fs.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/proc.h"
#include "string.h"
#include "lyos/global.h"
#include "lyos/proto.h"

PRIVATE int disp_pos = 0;

PRIVATE void put_char(const char c)
{
    char * video_mem = (char *)V_MEM_BASE; 
    video_mem[disp_pos++] = c;
    video_mem[disp_pos++] = 0x07;   /* grey on black */
}

PUBLIC void disp_char(const char c)
{
    if (c == '\n') {
        int space = SCR_WIDTH - ((disp_pos / 2) % SCR_WIDTH), i;
        for (i = 0; i < space; i++) 
            put_char(' ');
        return;
    }

    put_char(c);
}

PRIVATE void put_str(const char * str)
{
    while (*str)  {
        disp_char(*str);
        str++;
    }
}

PUBLIC int direct_print(const char * fmt, ...)
{
    int i;
    char buf[256];
    va_list arg;
    
    va_start(arg, fmt); 
    i = vsprintf(buf, fmt, arg);
    put_str(buf);

    va_end(arg);

    return i;
}

PUBLIC void direct_cls()
{
    int i;

    disp_pos = 0;

    for (i = 0; i < SCR_SIZE; i++) {
        put_char(' ');
    }

    disp_pos = 0;

    out_byte(CRTC_ADDR_REG, START_ADDR_H);
    out_byte(CRTC_DATA_REG, 0);
    out_byte(CRTC_ADDR_REG, START_ADDR_L);
    out_byte(CRTC_DATA_REG, 0);
}
