static struct xlat_data open_access_modes_data[] = {
    XLAT(O_RDONLY),
    XLAT(O_WRONLY),
    XLAT(O_RDWR),
    XLAT(O_ACCMODE),
};

static DEFINE_XLAT(open_access_modes);
