static struct xlat_data efd_flags_data[] = {
    XLAT(EFD_NONBLOCK),
    XLAT(EFD_CLOEXEC),
    XLAT(EFD_SEMAPHORE),
};

static DEFINE_XLAT(efd_flags);
