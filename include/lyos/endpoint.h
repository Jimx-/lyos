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

#ifndef _ENDPOINT_H_
#define _ENDPOINT_H_

#define ENDPOINT_GENERATION_SHIFT 10
#define ENDPOINT_GENERATION_SIZE (1 << ENDPOINT_GENERATION_SHIFT)

#define make_endpoint(gen, p) ((gen << ENDPOINT_GENERATION_SHIFT) + (p))
#define ENDPOINT_G(ep) (((ep) + NR_TASKS) >> ENDPOINT_GENERATION_SHIFT)
#define ENDPOINT_P(ep) \
    ((((ep) + NR_TASKS) & (ENDPOINT_GENERATION_SIZE - 1)) - NR_TASKS)

#endif
