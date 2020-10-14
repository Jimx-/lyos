static struct xlat_data socktypes_data[] = {
    XLAT(SOCK_DGRAM),     XLAT(SOCK_STREAM), XLAT(SOCK_RAW),    XLAT(SOCK_RDM),
    XLAT(SOCK_SEQPACKET), XLAT(SOCK_DCCP),   XLAT(SOCK_PACKET),
};

static DEFINE_XLAT(socktypes);
