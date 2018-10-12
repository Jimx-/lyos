#    Lyos CRT0
#
#    This file is part of Lyos.
#
#    Lyos is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.

.globl _start

.section .text

_start:
    # initialize global pointer
.option push
.option norelax
    la      gp, __global_pointer$
.option pop

    mv      s0, a0
    mv      s1, a1
    mv      s2, a2

    mv      a0, s2
    call	__lyos_init

    mv      a0, s0
    mv      a1, s1
    mv      a2, s2

    call		main

    call 		_exit
