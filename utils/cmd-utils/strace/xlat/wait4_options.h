static struct xlat_data wait4_options_data[] = {
    XLAT(WCONTINUED), XLAT(WNOHANG),  XLAT(WUNTRACED), XLAT(WEXITED),
    XLAT(WNOWAIT),    XLAT(WSTOPPED), XLAT(WCOREFLAG),
};

static DEFINE_XLAT(wait4_options);
