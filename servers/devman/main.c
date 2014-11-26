/*  
    (c)Copyright 2011 Jimx
    
    This file is part of Lyos.

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
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "proto.h"
#include "lyos/proc.h"
#include "lyos/driver.h"
#include <lyos/ipc.h>
#include <lyos/sysutils.h>

PRIVATE void devman_init();

PUBLIC int main(int argc, char * argv[])
{
	devman_init();
    
    MESSAGE msg;

    while (1) {
        send_recv(RECEIVE, ANY, &msg);
        int src = msg.source;

        switch (msg.type) {
        case ANNOUNCE_DEVICE:
            msg.RETVAL = do_announce_driver(&msg);
            break;
        case GET_DRIVER:
            msg.RETVAL = do_get_driver(&msg);
            break;
        default:
            printl("DEVMAN: unknown message type\n");
            break;
        }

        send_recv(SEND, src, &msg);
    }

    return 0;
}

PRIVATE void devman_init()
{
	printl("DEVMAN: Device manager is running.\n");

    init_dd_map();

    /* Map init ramdisk */
    map_driver(MAKE_DEV(DEV_RD, MINOR_INITRD), DT_BLOCKDEV, TASK_RD);

    //register_filesystem();
}
#if 0
PRIVATE int register_filesystem()
{
    MESSAGE m;

    /* register the filesystem */
    m.type = FS_REGISTER;
    m.PATHNAME = "devfs";
    m.NAME_LEN = strlen(m.PATHNAME);
    send_recv(BOTH, TASK_FS, &m);

    return 0;
}
#endif
