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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include <stdlib.h>
#include "unistd.h"
#include "assert.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "tty.h"
#include "console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/portio.h>
#include <lyos/sysutils.h>
#include <lyos/pci_utils.h>
#include <lyos/vm.h>
#include <lyos/timer.h>
#include <sys/mman.h>
#include "proto.h"
#include "global.h"

#include "fbcon_font.h"

#define VID_ARG 10
#define XRES_DEFAULT 1024
#define YRES_DEFAULT 768

int fb_scr_width, fb_scr_height;
static int cursor_state = 0;

extern int fbcon_init_bochs(int devind, int x_res, int y_res);

static void fbcon_outchar(CONSOLE* con, char ch);
static void fbcon_flush(CONSOLE* con);
static void update_cursor(struct timer_list* tp);

static struct timer_list cursor_timer;

int fbcon_init()
{
    int devind;
    u16 vid, did;
    int retval = pci_first_dev(&devind, &vid, &did, NULL);

    char val[VID_ARG];
    x_resolution = XRES_DEFAULT;
    y_resolution = YRES_DEFAULT;
    if (env_get_param("video", val, VID_ARG) == 0) {
        char* end;
        x_resolution = strtoul(val, &end, 10);
        if (*end != 'x')
            x_resolution = XRES_DEFAULT;
        else {
            end++;
            y_resolution = strtoul(end, &end, 10);
        }
    }

    fb_scr_height = y_resolution / FONT_HEIGHT;
    fb_scr_width = x_resolution / FONT_WIDTH;

    while (retval == 0) {
        if (vid == 0x1234 &&
            did == 0x1111) { /* 0x1234:0x1111: Bochs VBE Extension */
            return fbcon_init_bochs(devind, x_resolution, y_resolution);
        }

        retval = pci_next_dev(&devind, &vid, &did, NULL);
    }

    return 0;
}

static void set_pixel(int x, int y, int val)
{
    u32* vmem = (u32*)fb_mem_base;
    u32* pixel = &vmem[y * x_resolution + x];
    *pixel = val;
}

#define RGB_RED 0xaa0000
#define RGB_GREEN 0x00aa00
#define RGB_BLUE 0x0000aa
#define RGB_RED_BRI 0xff0000
#define RGB_GREEN_BRI 0x00ff00
#define RGB_BLUE_BRI 0x0000ff
static void color_to_rgb(u8 color, u8 attr, int* fg, int* bg)
{
    int r = (attr & BOLD) ? RGB_RED_BRI : RGB_RED;
    int g = (attr & BOLD) ? RGB_GREEN_BRI : RGB_GREEN;
    int b = (attr & BOLD) ? RGB_BLUE_BRI : RGB_BLUE;
    int fg_color = FG_COLOR(color), bg_color = BG_COLOR(color);
    *fg = *bg = 0;

    if (fg_color & RED) *fg |= r;
    if (fg_color & GREEN) *fg |= g;
    if (fg_color & BLUE) *fg |= b;

    if (bg_color & RED) *bg |= r;
    if (bg_color & GREEN) *bg |= g;
    if (bg_color & BLUE) *bg |= b;
}

static void print_char(CONSOLE* con, int x, int y, char ch)
{
    u8 color = con->color;
    int fg_color, bg_color;
    const u8* font = &number_font[ch * FONT_HEIGHT];
    int i, j;

    color_to_rgb(color, con->attributes, &fg_color, &bg_color);

    for (i = 0; i < FONT_HEIGHT; i++) {
        for (j = 0; j < FONT_WIDTH; j++) {
            if (font[i] & (1 << (FONT_WIDTH - j))) {
                set_pixel(x + j, y + i, fg_color);
            } else {
                set_pixel(x + j, y + i, bg_color);
            }
        }
    }
}

static void print_cursor(CONSOLE* con, int x, int y, int state)
{
    u8 color = state ? con->color
                     : MAKE_COLOR(BG_COLOR(con->color), BG_COLOR(con->color));
    int fg_color, bg_color;
    int i, j;

    color_to_rgb(color, con->attributes, &fg_color, &bg_color);

    for (i = 0; i < FONT_HEIGHT; i++) {
        for (j = 0; j < FONT_WIDTH; j++) {
            if (i > FONT_HEIGHT - CURSOR_HEIGHT) {
                set_pixel(x + j, y + i, fg_color);
            } else {
                set_pixel(x + j, y + i, bg_color);
            }
        }
    }
}

void fbcon_init_con(CONSOLE* con)
{
    con->cols = fb_scr_width;
    con->rows = fb_scr_height;
    con->xpixel = x_resolution;
    con->ypixel = y_resolution;

    con->origin = 0;
    con->visible_origin = con->cursor = con->origin;
    con->scr_end = con->origin + fb_scr_width * fb_scr_height;
    con->con_size =
        fb_scr_width * fb_scr_height * 2; /* * 2: extra space for scrolling */

    con->outchar = fbcon_outchar;
    con->flush = fbcon_flush;

    update_cursor(&cursor_timer);
}

static void fbcon_outchar(CONSOLE* con, char ch)
{
    int line, col;
    int cursor = con->cursor;

    if (ch == '\b') {
        cursor--;
        ch = '\0';
    }

    line = cursor / con->cols;
    col = cursor % con->cols;

    print_char(con, col * FONT_WIDTH, line * FONT_HEIGHT, ch);
}

static void fbcon_flush(CONSOLE* con)
{
    if (con->visible_origin != con->origin) {
        u32 delta = con->visible_origin - con->origin;

        u32 visible_origin = con->visible_origin;
        int line = visible_origin / con->cols;
        int col = visible_origin % con->cols;
        u32 offset = line * FONT_HEIGHT * x_resolution + col * FONT_WIDTH;

        u32* base = (u32*)fb_mem_base;
        u32* new_base = base + offset;
        memmove(base, new_base,
                (x_resolution * y_resolution - offset) * sizeof(u32));
        new_base = base + (x_resolution * y_resolution - offset);
        memset(new_base, 0, offset * sizeof(u32));

        con->visible_origin -= delta;
        con->scr_end -= delta;
        con->cursor -= delta;
    }
}

static void update_cursor(struct timer_list* tp)
{
    clock_t ticks = get_system_hz() / CURSOR_BLINK_RATE;
    cursor_state = ~cursor_state;

    int line, col;
    CONSOLE* con = &console_table[current_console];
    int cursor = con->cursor;

    line = cursor / con->cols;
    col = cursor % con->cols;

    print_cursor(con, col * FONT_WIDTH, line * FONT_HEIGHT, cursor_state);

    set_timer(tp, ticks, update_cursor, 0);
}
