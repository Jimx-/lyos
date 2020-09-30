static struct xlat_data mmap_flags_data[] = {
    XLAT(MAP_SHARED), XLAT(MAP_PRIVATE),  XLAT(MAP_ANONYMOUS),
    XLAT(MAP_FIXED),  XLAT(MAP_POPULATE), XLAT(MAP_CONTIG),
};

static DEFINE_XLAT(mmap_flags);
