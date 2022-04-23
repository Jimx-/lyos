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

#ifndef _ARCH_MACH_H_
#define _ARCH_MACH_H_

struct machine_desc {
    const char* name;
    const char* const* dt_compat;

    void (*init_machine)(void);
    void (*init_cpu)(unsigned int cpu);

    void (*serial_putc)(const char c);

    void (*handle_irq)(void);
    void (*handle_fiq)(void);
};

extern const struct machine_desc* machine_desc;

extern const struct machine_desc _arch_info_start[], _arch_info_end[];
#define for_each_machine_desc(p) \
    for (p = _arch_info_start; p < _arch_info_end; p++)

#define DT_MACHINE_START(_name, _namestr)                                      \
    static const struct machine_desc __mach_desc_##_name __attribute__((used)) \
    __attribute__((__section__(".arch.info.init"))) = {.name = _namestr,

#define MACHINE_END \
    }               \
    ;

#endif
