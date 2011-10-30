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
    
#ifndef _CTYPE_H
#define _CTYPE_H

#define isdigit(c)	((unsigned) ((c)-'0') < 10)
#define islower(c)	((unsigned) ((c)-'a') < 26)
#define isupper(c)	((unsigned) ((c)-'A') < 26)
#define isprint(c)	((unsigned) ((c)-' ') < 95)
#define isascii(c)	((unsigned) (c) < 128)

#endif /* _CTYPE_H */

