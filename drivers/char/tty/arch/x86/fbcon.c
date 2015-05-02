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

#include "fbcon_font.h"

#define VID_ARG     10
#define XRES_DEFAULT    1024
#define YRES_DEFAULT    768

int fb_scr_width, fb_scr_height;

PRIVATE void fbcon_outchar(CONSOLE * con, char ch);
PRIVATE void fbcon_flush(CONSOLE * con);

PUBLIC int fbcon_init()
{
    int devind;
    u16 vid, did;
    int retval = pci_first_dev(&devind, &vid, &did);

    char val[VID_ARG];
    x_resolution = XRES_DEFAULT;
    y_resolution = YRES_DEFAULT;
    if (env_get_param("video", val, VID_ARG) == 0) {
        char* end;
        x_resolution = strtoul(val, &end, 10);
        if (*end != 'x') x_resolution = XRES_DEFAULT;
        else {
            end++;
            y_resolution = strtoul(end, &end, 10);
        }
    }

    fb_scr_height = y_resolution / FONT_HEIGHT;
    fb_scr_width = x_resolution / FONT_WIDTH;

    while (retval == 0) {
        if (vid == 0x1234 && did == 0x1111) {   /* 0x1234:0x1111: Bochs VBE Extension */
            return fbcon_init_bochs(devind, x_resolution, y_resolution);
        }

        retval = pci_next_dev(&devind, &vid, &did);
    }

    return 0;
}

PRIVATE void set_pixel(int x, int y, int val)
{
    u32 * vmem = fb_mem_base;
    u32 * pixel = &vmem[y * x_resolution + x];
    *pixel = val;
}

PRIVATE void print_char(int x, int y, char ch, int color)
{
    u8 * font = number_font[ch];
    int i, j;
    for (i = 0; i < FONT_HEIGHT; i++) {
        for (j = 0; j < FONT_WIDTH; j++) {
            if (font[i] & (1 << (8-j))) {
                set_pixel(x+j, y+i, color);
            }
        }
    }
}

PUBLIC void fbcon_init_con(CONSOLE * con)
{
    con->cols = fb_scr_width;
    con->rows = fb_scr_height;

    con->origin = 0;
    con->visible_origin = con->cursor = con->origin;
    con->scr_end = con->origin + fb_scr_width * fb_scr_height;
    con->con_size = fb_scr_width * fb_scr_height * 2;   /* * 2: extra space for scrolling */

    con->outchar = fbcon_outchar;
    con->flush = fbcon_flush;
}

PRIVATE void fbcon_outchar(CONSOLE * con, char ch)
{
    int line, col;
    int cursor = con->cursor;

    if (ch == '\b') {
        cursor--;
        ch = '\0';
    }

    line = cursor / con->cols;
    col = cursor % con->cols;

    print_char(col * FONT_WIDTH, line * FONT_HEIGHT, ch, 0xffffffff);
}

PRIVATE void fbcon_flush(CONSOLE * con)
{
    if (con->visible_origin != con->origin) {
        u32 delta = con->visible_origin - con->origin;

        u32 visible_origin = con->visible_origin;
        int line = visible_origin / con->cols;
        int col = visible_origin % con->cols;
        u32 offset = line * FONT_HEIGHT * x_resolution + col * FONT_WIDTH;
        
        u32* base = (char*)fb_mem_base;
        u32* new_base = base + offset;
        memmove(base, new_base, (x_resolution * y_resolution - offset) * sizeof(u32));
        new_base = base + (x_resolution * y_resolution - offset);
        memset(new_base, 0, offset * sizeof(u32));

        con->visible_origin -= delta;
        con->scr_end -= delta;
        con->cursor -= delta;
    }
}
