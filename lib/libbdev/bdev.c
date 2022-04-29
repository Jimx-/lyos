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

#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <lyos/mgrant.h>

#include <libbdev/libbdev.h>
#include <libdevman/libdevman.h>

static endpoint_t driver_table[MAJOR_MAX];

void bdev_init()
{
    int i;
    for (i = 0; i < MAJOR_MAX; i++)
        driver_table[i] = NO_TASK;
}

static void bdev_set(dev_t dev)
{
    dev_t major = MAJOR(dev);

    if (major == DEV_RD)
        driver_table[major] = TASK_RD;
    else
        driver_table[major] = dm_get_bdev_driver(dev);

    if (driver_table[major] < 0) driver_table[major] = NO_TASK;
}

static void bdev_update(dev_t dev) { bdev_set(dev); }

int bdev_driver(dev_t dev)
{
    static int first = 1;

    if (first) {
        bdev_init();
    }

    bdev_update(dev);

    return 0;
}

static inline endpoint_t bdev_get_driver(dev_t dev)
{
    dev_t major = MAJOR(dev);
    return driver_table[major];
}

/*****************************************************************************
 *                                bdev_sendrec
 *****************************************************************************/
/**
 * Send a message to and receive the reply from the corresponding driver.
 *
 * @param dev      device nr
 * @param msg      Ptr to the message
 *
 * @return Zero if success.
 *****************************************************************************/
int bdev_sendrec(dev_t dev, MESSAGE* msg)
{
    endpoint_t driver_ep = bdev_get_driver(dev);
    if (driver_ep == NO_TASK) {
        bdev_update(dev);
        driver_ep = bdev_get_driver(dev);
    }

    return send_recv(BOTH, driver_ep, msg);
}

/*****************************************************************************
 *                                bdev_open
 *****************************************************************************/
/**
 * Open the device.
 *
 * @param dev      device nr
 *
 * @return Zero if success.
 *****************************************************************************/
int bdev_open(dev_t dev)
{
    MESSAGE driver_msg;

    driver_msg.type = BDEV_OPEN;
    driver_msg.u.m_bdev_blockdriver_msg.minor = MINOR(dev);

    bdev_sendrec(dev, &driver_msg);

    return driver_msg.RETVAL;
}

/*****************************************************************************
 *                                bdev_close
 *****************************************************************************/
/**
 * Close the device.
 *
 * @param dev      device nr
 *
 * @return Zero if success.
 *****************************************************************************/
int bdev_close(dev_t dev)
{
    MESSAGE driver_msg;

    driver_msg.type = BDEV_OPEN;
    driver_msg.u.m_bdev_blockdriver_msg.minor = MINOR(dev);

    bdev_sendrec(dev, &driver_msg);

    return driver_msg.u.m_blockdriver_bdev_reply.status;
}

/*****************************************************************************
 *                                bdev_readwrite
 *****************************************************************************/
/**
 * R/W a sector via messaging with the corresponding driver.
 *
 * @param io_type  DEV_READ or DEV_WRITE
 * @param dev      device nr
 * @param pos      Byte offset from/to where to r/w.
 * @param bytes    r/w count in bytes.
 * @param proc_nr  To whom the buffer belongs.
 * @param buf      r/w buffer.
 *
 * @return Bytes read/wrote or a negative error code.
 *****************************************************************************/
static ssize_t bdev_readwrite(int io_type, dev_t dev, loff_t pos, size_t bytes,
                              void* buf)
{
    MESSAGE driver_msg;
    endpoint_t driver_ep;
    mgrant_id_t grant;
    int access;

    if ((driver_ep = bdev_get_driver(dev)) == NO_TASK) return -EIO;

    access = (io_type == BDEV_READ) ? MGF_WRITE : MGF_READ;

    grant = mgrant_set_direct(driver_ep, (vir_bytes)buf, bytes, access);
    if (grant == GRANT_INVALID) return -EINVAL;

    driver_msg.type = io_type;
    driver_msg.u.m_bdev_blockdriver_msg.minor = MINOR(dev);
    driver_msg.u.m_bdev_blockdriver_msg.pos = pos;
    driver_msg.u.m_bdev_blockdriver_msg.count = bytes;
    driver_msg.u.m_bdev_blockdriver_msg.grant = grant;

    bdev_sendrec(dev, &driver_msg);

    mgrant_revoke(grant);

    return driver_msg.u.m_blockdriver_bdev_reply.status;
}

ssize_t bdev_read(dev_t dev, loff_t pos, void* buf, size_t count)
{
    return bdev_readwrite(BDEV_READ, dev, pos, count, buf);
}

ssize_t bdev_write(dev_t dev, loff_t pos, void* buf, size_t count)
{
    return bdev_readwrite(BDEV_WRITE, dev, pos, count, buf);
}

static ssize_t bdev_vreadwrite(int io_type, dev_t dev, loff_t pos,
                               const struct iovec* iov, size_t count)
{
    MESSAGE driver_msg;
    endpoint_t driver_ep;
    mgrant_id_t grant;
    struct iovec_grant giov[NR_IOREQS];
    int i, access;

    if ((driver_ep = bdev_get_driver(dev)) == NO_TASK) return -EIO;

    access = (io_type == BDEV_READV) ? MGF_WRITE : MGF_READ;

    for (i = 0; i < count; i++) {
        grant = mgrant_set_direct(driver_ep, (vir_bytes)iov[i].iov_base,
                                  iov[i].iov_len, access);
        if (grant == GRANT_INVALID) {
            for (i--; i >= 0; i--) {
                mgrant_revoke(giov[i].iov_grant);
            }
            return -EINVAL;
        }

        giov[i].iov_grant = grant;
        giov[i].iov_len = iov[i].iov_len;
    }

    grant = mgrant_set_direct(driver_ep, (vir_bytes)giov,
                              sizeof(giov[0]) * count, MGF_READ);
    if (grant == GRANT_INVALID) {
        for (i = 0; i < count; i++) {
            mgrant_revoke(giov[i].iov_grant);
        }
        return -EINVAL;
    }

    driver_msg.type = io_type;
    driver_msg.u.m_bdev_blockdriver_msg.minor = MINOR(dev);
    driver_msg.u.m_bdev_blockdriver_msg.pos = pos;
    driver_msg.u.m_bdev_blockdriver_msg.count = count;
    driver_msg.u.m_bdev_blockdriver_msg.grant = grant;

    bdev_sendrec(dev, &driver_msg);

    mgrant_revoke(grant);
    for (i = 0; i < count; i++) {
        mgrant_revoke(giov[i].iov_grant);
    }

    return driver_msg.u.m_blockdriver_bdev_reply.status;
}

ssize_t bdev_readv(dev_t dev, loff_t pos, const struct iovec* iov, size_t count)
{
    return bdev_vreadwrite(BDEV_READV, dev, pos, iov, count);
}

ssize_t bdev_writev(dev_t dev, loff_t pos, const struct iovec* iov,
                    size_t count)
{
    return bdev_vreadwrite(BDEV_WRITEV, dev, pos, iov, count);
}
