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


#include "lyos/lib-config.h"
#include "lyos/type.h"
#include "stdio.h"
#include "string.h"


#ifndef HAVE_ARCH_MEMCPY
PUBLIC	void*	memcpy(void* p_dst, void* p_src, int size)
{
        void * ret = p_dst;

        while (size--) {
                *(char *)p_dst = *(char *)p_src;
                p_dst = (char *)p_dst + 1;
                p_src = (char *)p_src + 1;
        }

        return(ret);
}
#endif

