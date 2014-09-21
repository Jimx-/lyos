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

#ifndef	_STACKFRAME_H_
#define	_STACKFRAME_H_

typedef u32 reg_t;

struct stackframe { 
    reg_t gs;     
    reg_t fs; 
    reg_t es; 
    reg_t ds; 
    reg_t edi;    
    reg_t esi;    
    reg_t ebp;    
    reg_t kernel_esp; 
    reg_t ebx;    
    reg_t edx;    
    reg_t ecx;    
    reg_t eax;    
    reg_t retaddr;
    reg_t eip;    
    reg_t cs; 
    reg_t eflags; 
    reg_t esp;    
    reg_t ss; 
};

#endif
