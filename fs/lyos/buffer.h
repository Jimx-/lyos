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

#ifndef _BUFFER_H
#define _BUFFER_H

struct buffer_head
{
	char * b_data;
	u64 b_pos;
	int b_dev;
	int b_count;
	unsigned short b_nr;
	unsigned char b_dirt;
	unsigned char b_lock;
	struct proc * b_wait;
	struct buffer_head * bh_prev;
	struct buffer_head * bh_next;
	struct buffer_head * bh_prev_free;
	struct buffer_head * bh_next_free;
};


#endif

