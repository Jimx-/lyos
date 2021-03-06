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

#ifndef _CHAR_PROTO_H_
#define _CHAR_PROTO_H_

void kb_init(TTY* tty);
void keyboard_read(TTY* p_tty);
int keyboard_interrupt(MESSAGE* m);
void handle_events(TTY* tty);
int in_process(TTY* p_tty, char* buf, int count);
void dump_tty_buf(); /* for debug only */
void do_input(MESSAGE* msg);
void init_keyboard();

void out_char(TTY* tty, char ch);
void scroll_screen(CONSOLE* p_con, int direction);
void select_console(int nr_console);
void init_screen(TTY* p_tty);
int is_current_console(CONSOLE* p_con);

int init_rs();
int rs_interrupt(MESSAGE* m);

#endif /* _CHAR_PROTO_H_ */
