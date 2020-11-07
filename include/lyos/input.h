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

#ifndef _INPUT_H_
#define _INPUT_H_

#include <uapi/linux/input.h>

typedef int input_dev_id_t;

struct input_value {
    u16 type;
    u16 code;
    s32 value;
};

struct input_dev_bits {
    bitchunk_t* evbit;
    bitchunk_t* keybit;
    bitchunk_t* relbit;
    bitchunk_t* absbit;
    bitchunk_t* mscbit;
    bitchunk_t* ledbit;
    bitchunk_t* sndbit;
    bitchunk_t* ffbit;
    bitchunk_t* swbit;
};

#endif
