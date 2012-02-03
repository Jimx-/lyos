
struct floppy_struct {
	unsigned int size, sect, head, track, stretch;
	unsigned char gap,rate,spec1,fmt_gap;
	char *name; /* used only for predefined formats */
};
