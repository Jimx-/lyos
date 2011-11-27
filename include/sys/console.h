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

#define MAX_ESC_PARAMS	4
/* CONSOLE */
typedef struct s_console
{
	unsigned int	crtc_start; /* set CRTC start addr reg */
	unsigned int	orig;	    /* start addr of the console */
	unsigned int	con_size;   /* how many words does the console have */
	unsigned int	cursor;
	
	char 			c_esc_state;		/* 0=normal, 1=ESC, 2=ESC[ */
	char 			c_esc_intro;		/* Distinguishing character following ESC */
	int *			c_esc_paramp;		/* pointer to current escape parameter */
	int 			c_esc_params[MAX_ESC_PARAMS];	/* list of escape parameters */
	int				is_full;
}CONSOLE;


#define SCR_UP	1	/* scroll upward */
#define SCR_DN	-1	/* scroll downward */

#define SCR_SIZE		(80 * 25)
#define SCR_WIDTH		 80

#define TAB_SIZE	8
#define TAB_MASK	7
#define DEFAULT_CHAR_COLOR	(MAKE_COLOR(BLACK, WHITE))
#define GRAY_CHAR		(MAKE_COLOR(BLACK, BLACK) | BRIGHT)
#define RED_CHAR		(MAKE_COLOR(BLUE, RED) | BRIGHT)

#endif /* _CONSOLE_H_ */
