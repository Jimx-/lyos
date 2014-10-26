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

#ifndef _TYPE_H_
#define _TYPE_H_

/* routine types */
#define	PUBLIC		/* PUBLIC is the opposite of PRIVATE */
#define	PRIVATE	static	/* PRIVATE x limits the scope of x */

#define EXTERN extern

typedef	unsigned long long	u64;
typedef	unsigned int		u32;
typedef	unsigned short		u16;
typedef	unsigned char		u8;

typedef unsigned int 		phys_bytes;
typedef unsigned int 		vir_bytes;
//#define __dev_t_defined
//typedef unsigned int        __dev_t;
typedef unsigned int        block_t;

typedef int 				endpoint_t;
typedef u32					bitchunk_t;

typedef	void	(*int_handler)	();
typedef	void	(*task_f)	();
typedef	void	(*irq_handler)	(int irq);

typedef void*	system_call;

struct hole {
	struct hole * h_next;
	int h_base;
	int h_len;
};

/**
 * MESSAGE mechanism is borrowed from MINIX
 */
struct mess1 {	/* 16 bytes */
	int m1i1;
	int m1i2;
	int m1i3;
	int m1i4;
};
struct mess2 {	/* 16 bytes */
	void* m2p1;
	void* m2p2;
	void* m2p3;
	void* m2p4;
};
struct mess3 {	/* 40 bytes */
	int	m3i1;
	int	m3i2;
	int	m3i3;
	int	m3i4;
	u64	m3l1;
	u64	m3l2;
	void*	m3p1;
	void*	m3p2;
};
struct mess4 {	/* 36 bytes */
	u64 m4l1;
	int m4i1, m4i2, m4i3; 
	void *m4p1, *m4p2, *m4p3, *m4p4; 
};
struct mess5 {	/* 40 bytes */
	int	m5i1;
	int	m5i2;
	int	m5i3;
	int	m5i4;
	int	m5i5;
	int	m5i6;
	int m5i7;
	int m5i8;
	int m5i9;
	int m5i10;
};

typedef struct {
	int source;
	int type;
	union {
		struct mess1 m1;
		struct mess2 m2;
		struct mess3 m3;
		struct mess4 m4;
		struct mess5 m5;
	} u;
} MESSAGE;

#endif /* _TYPE_H_ */
