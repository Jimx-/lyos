static struct xlat_data modetypes_data[] = {
    XLAT(S_IFREG), XLAT(S_IFDIR), XLAT(S_IFBLK),  XLAT(S_IFCHR),
    XLAT(S_IFLNK), XLAT(S_IFIFO), XLAT(S_IFSOCK),
};

static DEFINE_XLAT(modetypes);
