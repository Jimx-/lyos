static struct xlat_data pollflags_data[] = {
    XLAT(POLLIN),     XLAT(POLLPRI),    XLAT(POLLOUT),    XLAT(POLLERR),
    XLAT(POLLHUP),    XLAT(POLLNVAL),   XLAT(POLLRDNORM), XLAT(POLLRDBAND),
    XLAT(POLLWRNORM), XLAT(POLLWRBAND), XLAT(POLLMSG),    XLAT(POLLREMOVE),
    XLAT(POLLRDHUP),
};

static DEFINE_XLAT(pollflags);
