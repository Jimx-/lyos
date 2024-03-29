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

#include <lyos/ipc.h>
#include "stdio.h"
#include "stdarg.h"
#include "lyos/const.h"
#include <kernel/proto.h>
#include <asm/const.h>
#include <asm/page.h>

static int disp_pos = 0, print_line = 0;

#define V_MEM_BASE __va(0xB8000)

#define SCR_HEIGHT 25
#define SCR_WIDTH 80
#define SCR_SIZE (80 * 25)

static char get_char(int line, int col)
{
    int offset = line * SCR_WIDTH * 2 + col * 2;
    char* video_mem = (char*)V_MEM_BASE;
    return video_mem[offset];
}

static void put_char(const char c)
{
    char* video_mem = (char*)V_MEM_BASE;
    video_mem[disp_pos++] = c;
    video_mem[disp_pos++] = 0x07; /* grey on black */
}

static void put_char_at(const char c, int line, int col)
{
    int offset = line * SCR_WIDTH * 2 + col * 2;
    char* video_mem = (char*)V_MEM_BASE;
    video_mem[offset++] = c;
    video_mem[offset] = 0x07; /* grey on black */
}

static void scroll_up(int lines)
{
    int i, j;
    for (i = 0; i < SCR_HEIGHT; i++) {
        for (j = 0; j < SCR_WIDTH; j++) {
            char c = 0;
            if (i < SCR_HEIGHT - lines) c = get_char(i + lines, j);
            put_char_at(c, i, j);
        }
    }
    print_line -= lines;
    disp_pos -= lines * SCR_WIDTH * 2;
}

void disp_char(const char c)
{
    while (print_line >= SCR_HEIGHT)
        scroll_up(1);

    if (c == '\n') {
        int space = SCR_WIDTH - ((disp_pos / 2) % SCR_WIDTH), i;
        for (i = 0; i < space; i++)
            put_char(' ');
        print_line++;
        return;
    }

    put_char(c);
    if ((disp_pos / 2 + 1) % SCR_WIDTH == 0) print_line++;

    while (print_line >= SCR_HEIGHT)
        scroll_up(1);
}

void direct_put_str(const char* str)
{
    while (*str) {
        disp_char(*str);
        str++;
    }
}

int direct_print(const char* fmt, ...)
{
    int i;
    char buf[256];
    va_list arg;

    va_start(arg, fmt);
    i = vsprintf(buf, fmt, arg);
    direct_put_str(buf);

    va_end(arg);

    return i;
}

void direct_cls()
{
    int i;

    disp_pos = 0;

    for (i = 0; i < SCR_SIZE; i++) {
        put_char(' ');
    }

    disp_pos = 0;
    print_line = 0;

    out_byte(CRTC_ADDR_REG, START_ADDR_H);
    out_byte(CRTC_DATA_REG, 0);
    out_byte(CRTC_ADDR_REG, START_ADDR_L);
    out_byte(CRTC_DATA_REG, 0);
}
