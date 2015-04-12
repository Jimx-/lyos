static char * _progname;
void setprogname(const char *progname)
{
	_progname = progname;
}

const char* getprogname(void)
{
	return _progname;
}
