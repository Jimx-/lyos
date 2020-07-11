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

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#define MAX_ESC_PARAMS 4
/* CONSOLE */
typedef struct s_console {
    TTY* con_tty;
    unsigned int visible_origin; /* set CRTC start addr reg */
    unsigned int origin;         /* start addr of the console */
    unsigned int scr_end;
    unsigned int con_size; /* how many words does the console have */

    unsigned short* screenbuf;
    unsigned int screenbuf_size;
    unsigned int cursor;

    /* size */
    unsigned int row_size; /* bytes per row */
    unsigned int rows;     /* screen height in characters */
    unsigned int cols;     /* screen width in characters */
    unsigned int xpixel;   /* screen width in pixels */
    unsigned int ypixel;   /* screen height in pixels */

    /* attributes */
    unsigned char attributes;
    unsigned char color;
    unsigned char default_color;

    /* escape */
    char c_esc_state;  /* 0=normal, 1=ESC, 2=ESC[ */
    char c_esc_intro;  /* Distinguishing character following ESC */
    int* c_esc_paramp; /* pointer to current escape parameter */
    int c_esc_params[MAX_ESC_PARAMS]; /* list of escape parameters */
    int is_full;

    void (*outchar)(struct s_console* con, char ch);
    void (*flush)(struct s_console* con);
} CONSOLE;

/* Attributes */
#define BOLD 0x1
#define UNDERSCORE 0x2
#define BLINK 0x4

/* Color */
/*
 * e.g. MAKE_COLOR(BLUE, RED)
 *      MAKE_COLOR(BLACK, RED) | BRIGHT
 *      MAKE_COLOR(BLACK, RED) | BRIGHT | FLASH
 */
#define BLACK 0x0   /* 0000 */
#define WHITE 0x7   /* 0111 */
#define RED 0x1     /* 0001 */
#define GREEN 0x2   /* 0010 */
#define BLUE 0x4    /* 0100 */
#define FLASH 0x80  /* 1000 0000 */
#define BRIGHT 0x08 /* 0000 1000 */
#define MAKE_COLOR(x, y)                                \
    ((x << 4) | y) /* MAKE_COLOR(Background,Foreground) \
                    */
#define BG_COLOR(c) ((c >> 4) & 0xf)
#define FG_COLOR(c) (c & 0xf)

#define SCR_UP 1  /* scroll upward */
#define SCR_DN -1 /* scroll downward */

#define SCR_SIZE (80 * 25)
#define SCR_WIDTH 80

#define FONT_HEIGHT 16
#define FONT_WIDTH 8
#define CURSOR_HEIGHT 3

#define TAB_SIZE 8
#define TAB_MASK 7
#define DEFAULT_CHAR_COLOR (MAKE_COLOR(BLACK, WHITE))
#define GRAY_CHAR (MAKE_COLOR(BLACK, BLACK) | BRIGHT)
#define RED_CHAR (MAKE_COLOR(BLUE, RED) | BRIGHT)

#define CURSOR_BLINK_RATE 2 /* cursor blink rate (hz) */

void vgacon_init_con(CONSOLE* con);
void fbcon_init_con(CONSOLE* con);

#endif /* _CONSOLE_H_ */
