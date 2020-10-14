static struct xlat_data timerfdflags_data[] = {
    XLAT(TFD_TIMER_ABSTIME),
    XLAT(TFD_TIMER_CANCEL_ON_SET),
    XLAT(TFD_NONBLOCK),
    XLAT(TFD_CLOEXEC),
};

static DEFINE_XLAT(timerfdflags);
