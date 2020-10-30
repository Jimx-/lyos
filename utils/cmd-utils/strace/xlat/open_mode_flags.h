static struct xlat_data open_mode_flags_data[] = {
    XLAT(O_APPEND),   XLAT(O_CREAT),     XLAT(O_TRUNC),  XLAT(O_EXCL),
    XLAT(O_SYNC),     XLAT(O_NONBLOCK),  XLAT(O_NOCTTY), XLAT(O_CLOEXEC),
    XLAT(O_NOFOLLOW), XLAT(O_DIRECTORY),
};

static DEFINE_XLAT(open_mode_flags);
