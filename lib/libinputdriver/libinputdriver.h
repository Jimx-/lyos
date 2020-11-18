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

#include <lyos/types.h>
#include <lyos/bitmap.h>
#include <libdevman/libdevman.h>

struct inputdriver_dev;

struct inputdriver {
    void (*input_interrupt)(unsigned long irq_set);
    void (*input_other)(MESSAGE* m);
};

struct inputdriver_dev {
    input_dev_id_t id;
    device_id_t dev_id;
    struct inputdriver* driver;
    struct input_id input_id;
    int registered;

    bitchunk_t evbit[BITCHUNKS(EV_CNT)];
    bitchunk_t keybit[BITCHUNKS(KEY_CNT)];
    bitchunk_t relbit[BITCHUNKS(REL_CNT)];
    bitchunk_t absbit[BITCHUNKS(ABS_CNT)];
    bitchunk_t mscbit[BITCHUNKS(MSC_CNT)];
    bitchunk_t ledbit[BITCHUNKS(LED_CNT)];
    bitchunk_t sndbit[BITCHUNKS(SND_CNT)];
    bitchunk_t ffbit[BITCHUNKS(FF_CNT)];
    bitchunk_t swbit[BITCHUNKS(SW_CNT)];
};

int inputdriver_device_init(struct inputdriver_dev* dev,
                            struct inputdriver* drv, device_id_t parent);
int inputdriver_register_device(struct inputdriver_dev* dev);

int inputdriver_send_event(struct inputdriver_dev* dev, u16 type, u16 code,
                           int value);

int inputdriver_start(struct inputdriver* drv);

static inline int inputdriver_sync(struct inputdriver_dev* dev)
{
    return inputdriver_send_event(dev, EV_SYN, SYN_REPORT, 0);
}

#endif
