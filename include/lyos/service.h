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

#ifndef _SERVICE_H_
#define _SERVICE_H_

struct service_up_req {
    char * cmdline;
    int cmdlen;
};

#define SERVICE_INIT_FRESH 0x1

typedef int (*serv_init_fresh_callback_t)();

PUBLIC void serv_register_init_fresh_callback(serv_init_fresh_callback_t cb);
PUBLIC int serv_init();

#endif
