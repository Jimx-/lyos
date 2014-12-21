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
#include <errno.h>
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include "signal.h"
#include "page.h"
#include "arch_const.h"
#include "arch_proto.h"
#ifdef CONFIG_SMP
#include "arch_smp.h"
#endif
#include "lyos/cpulocals.h"

PRIVATE struct proc * pick_proc();
PRIVATE void idle();
PUBLIC 	int  msg_send(struct proc* p_to_send, int des, MESSAGE* m);
PRIVATE int  msg_receive(struct proc* p_to_recv, int src, MESSAGE* m);
PRIVATE int  has_pending_notify(struct proc * p, endpoint_t src);
PRIVATE void unset_notify_pending(struct proc * p, int id);
PRIVATE void set_notify_msg(struct proc * sender, MESSAGE * m, endpoint_t src);
PRIVATE int  deadlock(int src, int dest);
PRIVATE void proc_no_time(struct proc * p);

/*****************************************************************************
 *                                pick_proc
 *****************************************************************************/
/**
 * <Ring 0> Choose one proc to run.
 * 
 *****************************************************************************/
PRIVATE struct proc * pick_proc()
{
  	register struct proc *p;
  	struct proc ** rq_head;
  	int q;

  	rq_head = get_cpulocal_var(run_queue_head);
  	for (q = 0; q < NR_SCHED_QUEUES; q++) {	
		if(!(p = rq_head[q])) {
			continue;
		}
		return p;
  	}
  	
  	return NULL;
}

PUBLIC void init_proc()
{
	int i;
	struct proc * p = proc_table;
	struct priv * priv;
	int prio, quantum;

	for (i = -NR_KERNTASKS; i < NR_TASKS + NR_PROCS; i++,p++) {
		spinlock_init(&p->lock);
		
		if (i >= (NR_BOOT_PROCS - NR_KERNTASKS)) {
			p->state = PST_FREE_SLOT;
			continue;
		}

		if (i == INIT) {
			prio    = USER_PRIO;
			quantum = USER_QUANTUM;
		} else {
			prio    = TASK_PRIO;
			quantum = TASK_QUANTUM;
		}

		p->counter = p->quantum_ms = quantum;
		p->priority = prio;

		p->p_parent = NO_TASK;
		p->endpoint = make_endpoint(0, i);	/* generation 0 */

		p->send_msg = 0;
		p->recv_msg = 0;
		p->recvfrom = NO_TASK;
		p->sendto = NO_TASK;
		p->q_sending = 0;
		p->next_sending = 0;

		p->state = 0;

		if (i != TASK_MM) {
			PST_SET(p, PST_BOOTINHIBIT);
			PST_SET(p, PST_MMINHIBIT);		/* process is not ready until MM allows it to run */
		}

		PST_SET(p, PST_STOPPED);
	}

	int id = 0;
	for (priv = &FIRST_PRIV; priv < &LAST_PRIV; priv++) {
		memset(priv, 0, sizeof(struct priv));
		priv->proc_nr = NO_TASK;
		priv->id = id++;
	}

	/* prepare idle process struct */
	
	for (i = 0; i < CONFIG_SMP_MAX_CPUS; i++) {
		struct proc * p = get_cpu_var_ptr(i, idle_proc);
		p->state |= PST_STOPPED;
		sprintf(p->name, "idle%d", i);
	}
}

/**
 * <Ring 0> Switch back to user space.
 */
PUBLIC void switch_to_user()
{
	struct proc * p = get_cpulocal_var(proc_ptr);

	if (proc_is_runnable(p)) goto no_schedule;

reschedule:
	while (!(p = pick_proc())) idle();

no_schedule:

	get_cpulocal_var(proc_ptr) = p;

	if (p->counter <= 0) proc_no_time(p);
	if (!proc_is_runnable(p)) goto reschedule;

	switch_address_space(p);

	p = arch_switch_to_user();

	restore_user_context(p);
}

PRIVATE void switch_address_space_idle()
{
#if CONFIG_SMP
	switch_address_space(proc_addr(TASK_MM));
#endif
}

/**
 * <Ring 0> Called when there is no work to do. Halt the CPU.
 */
PRIVATE void idle()
{
	get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
	switch_address_space_idle();

#if CONFIG_SMP
	get_cpulocal_var(cpu_is_idle) = 1;
#endif

	halt_cpu();
}

/*****************************************************************************
 *                                sys_sendrec
 *****************************************************************************/
/**
 * <Ring 0> The core routine of system call `sendrec()'.
 * 
 * @param function SEND or RECEIVE
 * @param src_dest To/From whom the message is transferred.
 * @param m        Ptr to the MESSAGE body.
 * @param p        The caller proc.
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int sys_sendrec(MESSAGE* m, struct proc* p)
{
	int function = m->SR_FUNCTION;
	int src_dest = m->SR_SRCDEST;
	MESSAGE * msg = (MESSAGE *)m->SR_MSG;

	int ret = 0;
	int caller = p->endpoint;
	MESSAGE * mla = (MESSAGE * )va2la(caller, msg);
	mla->source = caller;

	if (!verify_endpt(src_dest, NULL)) return EINVAL;

	assert(mla->source != src_dest);

	switch (function) {
		case BOTH:
			/* fall through */
		case SEND:
			ret = msg_send(p, src_dest, msg);
			if (ret != 0 || function == SEND) break;
			/* fall through for BOTH */
		case RECEIVE:
			ret = msg_receive(p, src_dest, msg);
			break;
		default:
			ret = EINVAL;
			break;
	}

	return ret;
}

/*****************************************************************************
 *                                reset_msg
 *****************************************************************************/
/**
 * <Ring 0~3> Clear up a MESSAGE by setting each byte to 0.
 * 
 * @param p  The message to be cleared.
 *****************************************************************************/
PUBLIC void reset_msg(MESSAGE* p)
{
	memset(p, 0, sizeof(MESSAGE));
}

PUBLIC struct proc * endpt_proc(endpoint_t ep)
{
	int n;
	if (!verify_endpt(ep, &n)) return NULL;

	return proc_addr(n);
}

/*****************************************************************************
 *                                deadlock
 *****************************************************************************/
/**
 * <Ring 0> Check whether it is safe to send a message from src to dest.
 * The routine will detect if the messaging graph contains a cycle. For
 * instance, if we have procs trying to send messages like this:
 * A -> B -> C -> A, then a deadlock occurs, because all of them will
 * wait forever. If no cycles detected, it is considered as safe.
 * 
 * @param src   Who wants to send message.
 * @param dest  To whom the message is sent.
 * 
 * @return Zero if success.
 *****************************************************************************/
PRIVATE int deadlock(endpoint_t src, endpoint_t dest)
{
	struct proc * p = endpt_proc(dest);

	while (TRUE) {
		if (p->state & PST_SENDING) {
			if (p->sendto == src) {
				p = endpt_proc(dest);
				do {
					p = endpt_proc(p->sendto);
				} while (p != endpt_proc(src));
				return 1;
			}
			p = endpt_proc(p->sendto);
		}
		else {
			break;
		}
	}
	return 0;
}

/*****************************************************************************
 *                                msg_send
 *****************************************************************************/
/**
 * <Ring 0> Send a message to the dest proc. If dest is blocked waiting for
 * the message, copy the message to it and unblock dest. Otherwise the caller
 * will be blocked and appended to the dest's sending queue.
 * 
 * @param p_to_send  The caller, the sender.
 * @param dest     To whom the message is sent.
 * @param m        The message.
 * 
 * @return Zero if success.
 *****************************************************************************/
PUBLIC int msg_send(struct proc* p_to_send, int dest, MESSAGE* m)
{
	struct proc * sender = p_to_send;
	struct proc * p_dest = endpt_proc(dest); /* proc dest */
	if (p_dest == NULL) return EINVAL;

	/* check for deadlock here */
	if (deadlock(sender->endpoint, dest)) {
		dump_msg("deadlock sender", m);
		dump_msg("deadlock receiver", p_dest->send_msg);
		return EDEADLK;
	}

	if (!PST_IS_SET(p_dest, PST_SENDING) && PST_IS_SET(p_dest, PST_RECEIVING) && /* p_dest is waiting for the msg */
	    (p_dest->recvfrom == sender->endpoint ||
	     p_dest->recvfrom == ANY)) {

		vir_copy(dest, p_dest->recv_msg, sender->endpoint, m, sizeof(MESSAGE));

		p_dest->recv_msg = 0;
		PST_UNSET(p_dest, PST_RECEIVING);
		p_dest->recvfrom = NO_TASK;
	}
	else { /* p_dest is not waiting for the msg */
		PST_SET(sender, PST_SENDING);
		sender->sendto = dest;
		sender->send_msg = m;

		/* append to the sending queue */
		struct proc * p;
		if (p_dest->q_sending) {
			p = p_dest->q_sending;
			while (p->next_sending)
				p = p->next_sending;
			p->next_sending = sender;
		}
		else {
			p_dest->q_sending = sender;
		}
		sender->next_sending = 0;
	}

	return 0;
}

/*****************************************************************************
 *                                msg_receive
 *****************************************************************************/
/**
 * <Ring 0> Try to get a message from the src proc. If src is blocked sending
 * the message, copy the message from it and unblock src. Otherwise the caller
 * will be blocked.
 * 
 * @param p_to_recv The caller, the proc who wanna receive.
 * @param src     From whom the message will be received.
 * @param m       The message ptr to accept the message.
 * 
 * @return  Zero if success.
 *****************************************************************************/
PRIVATE int msg_receive(struct proc* p_to_recv, int src, MESSAGE* m)
{
	struct proc* who_wanna_recv = p_to_recv; /**
						  * This name is a little bit
						  * wierd, but it makes me
						  * think clearly, so I keep
						  * it.
						  */
	struct proc* from = NULL; /* from which the message will be fetched */
	struct proc* prev = NULL;
	int copyok = 0;

	/* PST_SENDING is set means that the process failed to send a 
	 * message in sendrec(BOTH), simply block it.
	 */
	if (PST_IS_SET(who_wanna_recv, PST_SENDING)) goto no_msg;

	/* check pending notifications */
	int notify_id;
	if ((notify_id = has_pending_notify(who_wanna_recv, src)) != PRIV_ID_NULL) {
		MESSAGE msg;
		reset_msg(&msg);

		int proc_nr = priv_addr(notify_id)->proc_nr;
		struct proc * notifier = proc_addr(proc_nr);
		set_notify_msg(who_wanna_recv, &msg, notifier->endpoint);

		unset_notify_pending(who_wanna_recv, notify_id);
		memcpy(m, &msg, sizeof(MESSAGE));

		who_wanna_recv->recv_msg = NULL;
		PST_UNSET(who_wanna_recv, PST_RECEIVING);
		who_wanna_recv->recvfrom = NO_TASK;

		return 0;
	}

normal_msg:
	/* Arrives here if no interrupt for who_wanna_recv. */
	if (src == ANY) {
		/* who_wanna_recv is ready to receive messages from
		 * ANY proc, we'll check the sending queue and pick the
		 * first proc in it.
		 */
		if (who_wanna_recv->q_sending) {
			from = who_wanna_recv->q_sending;
			copyok = 1;
		}
	}
	else {
		/* who_wanna_recv wants to receive a message from
		 * a certain proc: src.
		 */
		from = endpt_proc(src);
		if (from == NULL) return EINVAL;

		if ((PST_IS_SET(from, PST_SENDING)) &&
		    (from->sendto == who_wanna_recv->endpoint)) {
			/* Perfect, src is sending a message to
			 * who_wanna_recv.
			 */
			copyok = 1;

			struct proc* p = who_wanna_recv->q_sending;
			assert(p); /* from must have been appended to the
				    * queue, so the queue must not be NULL
				    */
			while (p) {
				if (p->endpoint == src) { /* if p is the one */
					from = p;
					break;
				}
				prev = p;
				p = p->next_sending;
			}
		}
	}

	if (copyok) {
		/* It's determined from which proc the message will
		 * be copied. Note that this proc must have been
		 * waiting for this moment in the queue, so we should
		 * remove it from the queue.
		 */
		if (from == who_wanna_recv->q_sending) { /* the 1st one */
			assert(prev == 0);
			who_wanna_recv->q_sending = from->next_sending;
			from->next_sending = 0;
		}
		else {
			assert(prev);
			prev->next_sending = from->next_sending;
			from->next_sending = 0;
		}

		assert(m);
		/* copy the message */
		vir_copy(who_wanna_recv->endpoint, m, from->endpoint, from->send_msg, sizeof(MESSAGE));

		from->send_msg = 0;
		from->sendto = NO_TASK;
		PST_UNSET(from, PST_SENDING);

		return 0;
	}

no_msg:
	/* nobody's sending any msg */
	/* Set state so that who_wanna_recv will not
	* be scheduled until it is unblocked.
	*/
	PST_SET(who_wanna_recv, PST_RECEIVING);

	who_wanna_recv->recv_msg = m;
	who_wanna_recv->recvfrom = src;

	return 0;
}

PRIVATE int has_pending_notify(struct proc * p, endpoint_t src)
{
	priv_map_t notify_pending = p->priv->notify_pending;
	int i;

	if (notify_pending == 0) return PRIV_ID_NULL;

	if (src != ANY) {
		struct proc * sender = endpt_proc(src);
		if (!sender) return PRIV_ID_NULL;

		if (notify_pending & (1 << sender->priv->id)) return sender->priv->id; 
		else return PRIV_ID_NULL;
	}

	for (i = 0; i < NR_PRIV_PROCS; i++) {
		if (notify_pending & (1 << i)) return i;
	}

	return PRIV_ID_NULL;
}

PRIVATE void unset_notify_pending(struct proc * p, int id)
{
	p->priv->notify_pending &= ~(1 << id);
}

PRIVATE void set_notify_msg(struct proc * dest, MESSAGE * m, endpoint_t src)
{
	memset(m, 0, sizeof(MESSAGE));
	m->source = src;
	m->type = NOTIFY_MSG;
	switch (src) {
	case INTERRUPT:
		m->INTERRUPTS = dest->priv->int_pending;
		dest->priv->int_pending = 0;
		break;
	default:
		break;
	}
}

/**
 * @brief Send a notification to the dest proc.
 * 
 * @param p_to_send	Who wants to send the notification.
 * @param dest The dest proc.
 * 
 * @return Zero on success, otherwise errcode.
 */
PUBLIC int msg_notify(struct proc * p_to_send, endpoint_t dest)
{
	struct proc * p_dest = endpt_proc(dest);
	if (!p_dest) return EINVAL;

	if (!PST_IS_SET(p_dest, PST_SENDING) && PST_IS_SET(p_dest, PST_RECEIVING) && /* p_dest is waiting for the msg */
	    (p_dest->recvfrom == p_to_send->endpoint ||
	     p_dest->recvfrom == ANY)) {
		
		MESSAGE m;
		set_notify_msg(p_dest, &m, p_to_send->endpoint);
		vir_copy(dest, p_dest->recv_msg, p_to_send->endpoint, &m, sizeof(MESSAGE));

		p_dest->recv_msg = NULL;
		PST_UNSET(p_dest, PST_RECEIVING);
		p_dest->recvfrom = NO_TASK;

		return 0;
	}

	/* p_dest is not waiting for this notification, set pending bit */
	p_dest->priv->notify_pending |= (1 << p_to_send->priv->id);
	return 0;
}

/**
 * @brief Verify if an endpoint number is valid and convert it to proc nr.
 * 
 * @param ep Endpoint number.
 * @param proc_nr [out] Ptr to proc nr.
 * 
 * @return Non-zero if the endpoint number is valid.
 */
PUBLIC int verify_endpt(endpoint_t ep, int * proc_nr)
{
	if (proc_nr) *proc_nr = ENDPOINT_P(ep);

	return 1;
}

/*****************************************************************************
 *                                dumproc
 *****************************************************************************/
PUBLIC void dumproc(struct proc* p)
{
#if 0
	sprintf(info, "counter: 0x%x.  ", p->counter); disp_color_str(info, text_color);
	sprintf(info, "priority: 0x%x.  ", p->priority); disp_color_str(info, text_color); 

	sprintf(info, "name: %s.  ", p->name); disp_color_str(info, text_color);
	disp_color_str("\n", text_color);
	sprintf(info, "state: 0x%x.  ", p->state); disp_color_str(info, text_color);
	sprintf(info, "recvfrom: 0x%x.  ", p->recvfrom); disp_color_str(info, text_color);
	sprintf(info, "sendto: 0x%x.  ", p->sendto); disp_color_str(info, text_color);

	disp_color_str("\n", text_color);
	sprintf(info, "special_msg: 0x%x.  ", p->special_msg); disp_color_str(info, text_color);
#endif
}


/*****************************************************************************
 *                                dummsg
 *****************************************************************************/
PUBLIC void dump_msg(const char * title, MESSAGE* m)
{
	int packed = 0;
	printk("\n\n%s<0x%x>{%ssrc:%d,%stype:%d,%sm->u.m3:{0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x}%s}%s",  //, (0x%x, 0x%x, 0x%x)}",
	       title,
	       (int)m,
	       packed ? "" : "\n        ",
	       m->source,
	       packed ? " " : "\n        ",
	       m->type,
	       packed ? " " : "\n        ",
	       m->u.m3.m3i1,
	       m->u.m3.m3i2,
	       m->u.m3.m3i3,
	       m->u.m3.m3i4,
	       (int)m->u.m3.m3p1,
	       (int)m->u.m3.m3p2,
	       packed ? "" : "\n",
	       packed ? "" : "\n"/* , */
		);
}

/**
 * <Ring 0> Insert a process into scheduling queue.
 */
PUBLIC void enqueue_proc(register struct proc * p)
{
	int prio = p->priority;
	struct proc ** rq_head, ** rq_tail;

	rq_head = get_cpu_var(p->cpu, run_queue_head);
  	rq_tail = get_cpu_var(p->cpu, run_queue_tail);

  	if (!rq_head[prio]) {
  		rq_head[prio] = rq_tail[prio] = p;
  		p->next_ready = NULL;
  	} else {
  		rq_tail[prio]->next_ready = p;
  		rq_tail[prio] = p;
  		p->next_ready = NULL;
  	}
}

/**
 * <Ring 0> Remove a process from scheduling queue.
 */
PUBLIC void dequeue_proc(register struct proc * p)
{
	int q = p->priority;	
  	struct proc ** xpp;
  	struct proc * prev_xp;

  	struct proc ** rq_tail;

  	rq_tail = get_cpu_var(p->cpu, run_queue_tail);

  	prev_xp = NULL;				
  	for (xpp = get_cpu_var_ptr(p->cpu, run_queue_head[q]); *xpp;
		  	xpp = &(*xpp)->next_ready) {
      	if (*xpp == p) {
          	*xpp = (*xpp)->next_ready;
          	if (p == rq_tail[q]) {
              	rq_tail[q] = prev_xp;
	  		}

        	break;
    	}
      	prev_xp = *xpp;
  	}
}

/**
 * <Ring 0> Called when a process has run out its counter.
 */
PRIVATE void proc_no_time(struct proc * p)
{
	p->counter = p->quantum_ms;
}
