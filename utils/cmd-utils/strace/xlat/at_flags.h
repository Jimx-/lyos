static struct xlat_data at_flags_data[] = {
    XLAT(AT_SYMLINK_NOFOLLOW),
    XLAT(AT_SYMLINK_FOLLOW),
    XLAT(AT_REMOVEDIR),
};

static DEFINE_XLAT(at_flags);
