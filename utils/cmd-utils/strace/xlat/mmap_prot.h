static struct xlat_data mmap_prot_data[] = {
    XLAT(PROT_NONE),
    XLAT(PROT_READ),
    XLAT(PROT_WRITE),
    XLAT(PROT_EXEC),
};

static DEFINE_XLAT(mmap_prot);
