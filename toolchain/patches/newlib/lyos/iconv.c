#include <iconv.h>

size_t iconv(iconv_t cd, char** __restrict inbuf,
             size_t* __restrict inbytesleft, char** __restrict outbuf,
             size_t* __restrict outbytesleft)
{
    return 0;
}

int iconv_close(iconv_t cd) { return 0; }

iconv_t iconv_open(const char* tocode, const char* fromcode) { return 0; }
