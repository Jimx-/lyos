#    Lyos CRT0
#
#    This file is part of Lyos.
#
#    Lyos is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.

.global _start
.extern	main
.extern	exit

.section .text

_start:
	push	{r0-r2}
	bl		__lyos_init
	pop 	{r0-r2}

	bl		main

	bl 		_exit
