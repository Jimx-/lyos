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

#include "type.h"
#include "config.h"
#include "stdio.h"
#include "unistd.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"
#include "hd.h"
#include "buffer.h"

extern int end;
#define start_buffer (struct buffer_head *)buffer_base

PUBLIC char * get_buffer(int dev, u64 pos, int cnt)
{
	struct buffer_head * bh = start_buffer;

	for(;bh->bh_next;bh = bh->bh_next){
		if (bh->b_dirt == 0){
			bh->b_dev = dev;
			bh->b_pos = pos;
			bh->b_count = cnt;
			bh->b_dirt = 1;
			return bh->b_data;}
	}
	free_buffer();
	return 0;
}

PUBLIC char * find_buffer(int dev, u64 pos, int cnt)
{
	struct buffer_head * bh = start_buffer;
	
	for(;bh->bh_next;bh = bh->bh_next){
		if ((bh->b_dev == dev) && (bh->b_pos == pos) &&(bh->b_count == cnt)){ bh->b_dev = 0;return bh->b_data;}
	}
	return 0;
}

PUBLIC void 	sync_buffer()
{
}

PUBLIC void 	do_sync()
{
}


PUBLIC void 	free_buffer()
{	
}

PUBLIC void init_buffer()
{
	struct buffer_head * s = start_buffer;
	void * e;
	int i = 0;

	e = (void *)(buffer_base + (1024 * 640) + buffer_length);

	while ((e -= 512) >= ((void *)(buffer_base +  (1024 * 640))))
	{
		s->b_dev = 0;
		s->b_pos = 0;
		s->b_dirt = 0;		
		s->b_count = 0;
		s->b_data = (char *)e;
		s->bh_prev = s-1;
		s->bh_next = s+1;
		//printl("buffer_table[%d]->b_data:%d\n",i-1, (s->bh_prev)->b_data);
		//printl("buffer_table[%d]->b_data:%d\n",i, s->b_data);
		s++;
		i++; 
	}

	printl("%d buffers, space = %d bytes(%d kB), start at 0x%x\n", i, i * 512, (i* 512) / 1024, buffer_base);
	printl("size of buffer heads = %d bytes(%d kB)\n", i * sizeof(struct buffer_head), (i* sizeof(struct buffer_head)) / 1024);
	/*printl("[%d-%d(%dMB-%dMB) for buffer heads\n", BUFFER_BASE, BUFFER_BASE + (1024 * 1024), BUFFER_BASE/1024/1024, (BUFFER_BASE + (1024 * 1024))/1024/1024);
	printl("%d-%d(%dMB-%dMB) for buffer data]\n\n", BUFFER_BASE + (1024 * 1024), BUFFER_BASE + (1024 * 1024) + (1024 * 640),
					     (BUFFER_BASE + (1024 * 1024))/1024/1024, (BUFFER_BASE + (1024 * 1024) + (1024 * 640))/1024/1024);*/
}

