#include "lyos/type.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/const.h"
#include "lyos/protect.h"
#include "string.h"
#include "lyos/fs.h"
#include "lyos/proc.h"
#include "lyos/tty.h"
#include "lyos/console.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "lyos/hd.h"
#include "blk.h"

extern	struct request	requests[NR_REQUEST];
struct blk_dev	blk_dev_table[NR_BLK_DEV] = {
			{0, 0},			/* no dev */
			{0, 0},			/* dev mem */
			{0, 0},			/* dev fd */
			{0, 0},			/* dev hd */
			{0, 0},			/* dev ttyx */
			{0, 0},			/* dev tty */
			{0, 0}			/* dev lp */
};

/*****************************************************************************
 *                                add_request
 *****************************************************************************/
/**
 * Add a request to the request table.
 *
 *****************************************************************************/
PUBLIC void	add_request(dev_t dev, MESSAGE * m)
{
	struct request * req = 0;
	struct request * tmp;

	req->p = m;
	req->free = 0;
	req->next = 0;

	tmp = blk_dev_table[dev].r_current;

	if(!blk_dev_table[dev].r_current){
		(blk_dev_table[dev].r_current)->p = req->p;
		(blk_dev_table[dev].rq_handle)();
		return;
	}
	int i = 0;

	for (;tmp->next;tmp = tmp->next){
		i++;
		if (!tmp->next) break;}

	tmp->next = req;
}
