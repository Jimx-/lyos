.section ".init", "ax", %progbits
	ldmea	fp, {fp, sp, pc}

.section ".fini", "ax", %progbits
	ldmea	fp, {fp, sp, pc}
	