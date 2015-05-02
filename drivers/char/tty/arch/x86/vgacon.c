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
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "tty.h"
#include "console.h"
#include "lyos/global.h"
#include "keyboard.h"
#include "lyos/proto.h"
#include <lyos/portio.h>
#include <lyos/vm.h>
#include <sys/mman.h>
#include "proto.h"
#include "global.h"

#define V_MEM_BASE  0xB8000 /* base of color video memory */
#define V_MEM_SIZE  0x8000  /* 32K: B8000H -> BFFFFH */

PRIVATE void vga_outchar(CONSOLE * con, char ch);
PRIVATE void vga_flush(CONSOLE * con);

PUBLIC void vgacon_init_con(CONSOLE * con)
{
    int v_mem_size = V_MEM_SIZE >> 1; /* size of Video Memory */
    int size_per_con = v_mem_size / NR_CONSOLES;
    con->origin = (con - console_table) * size_per_con;
    con->cols = SCR_WIDTH;
    con->rows = SCR_SIZE / SCR_WIDTH;
    con->con_size = size_per_con / con->cols * con->cols;
    con->cursor = con->visible_origin = con->origin;
    con->scr_end = con->origin + SCR_SIZE;

    con->outchar = vga_outchar;
    con->flush = vga_flush;
}

PRIVATE void vga_outchar(CONSOLE * con, char ch)
{
    u8* pch = (u8*)(console_mem + con->cursor * 2);

    switch (ch) {
    case '\b':
        *(pch - 2) = ' ';
        *(pch - 1) = DEFAULT_CHAR_COLOR;
        break;
    default:
        *pch++ = ch;
        *pch++ = DEFAULT_CHAR_COLOR;
        break;
    }
}

/*****************************************************************************
 *                                set_cursor
 *****************************************************************************/
/**
 * Display the cursor by setting CRTC (6845 compatible) registers.
 * 
 * @param position  Position of the cursor based on the beginning of the video
 *                  memory. Note that it counts in WORDs, not in BYTEs.
 *****************************************************************************/
PRIVATE void set_cursor(unsigned int position)
{
    pb_pair_t pv_pairs[4];
    pv_set(pv_pairs[0], CRTC_ADDR_REG, CURSOR_H);
    pv_set(pv_pairs[1], CRTC_DATA_REG, (position >> 8) & 0xFF);
    pv_set(pv_pairs[2], CRTC_ADDR_REG, CURSOR_L);
    pv_set(pv_pairs[3], CRTC_DATA_REG, position & 0xFF);
    portio_voutb(pv_pairs, 4);
}


/*****************************************************************************
 *                                set_video_start_addr
 *****************************************************************************/
/**
 * Routine for hardware screen scrolling.
 * 
 * @param addr  Offset in the video memory.
 *****************************************************************************/
PRIVATE void set_video_start_addr(u32 addr)
{
    pb_pair_t pv_pairs[4];
    pv_set(pv_pairs[0], CRTC_ADDR_REG, START_ADDR_H);
    pv_set(pv_pairs[1], CRTC_DATA_REG, (addr >> 8) & 0xFF);
    pv_set(pv_pairs[2], CRTC_ADDR_REG, START_ADDR_L);
    pv_set(pv_pairs[3], CRTC_DATA_REG, addr & 0xFF);
    portio_voutb(pv_pairs, 4);
}

PRIVATE void vga_flush(CONSOLE * con)
{
    set_cursor(con->cursor);
    set_video_start_addr(con->visible_origin);
}
