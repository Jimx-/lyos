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

#ifndef _LIBINPUTDRIVER_H_
#define _LIBINPUTDRIVER_H_


struct inputdriver {
    void (*input_interrupt)(unsigned long irq_set);
    int (*input_other)(MESSAGE* m);
};

PUBLIC int inputdriver_start(struct inputdriver* inpd);
PUBLIC int inputdriver_send_event(u16 type, u16 code, int value);

#endif
