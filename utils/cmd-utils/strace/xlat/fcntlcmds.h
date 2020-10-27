static struct xlat_data fcntlcmds_data[] = {
    XLAT(F_DUPFD), XLAT(F_GETFD),  XLAT(F_SETFD),
    XLAT(F_GETFL), XLAT(F_SETFL),  XLAT(F_GETLK),
    XLAT(F_SETLK), XLAT(F_SETLKW), XLAT(F_DUPFD_CLOEXEC),
};

static DEFINE_XLAT(fcntlcmds);
