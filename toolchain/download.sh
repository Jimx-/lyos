#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/utils.sh

pushd "$DIR" > /dev/null

if [ ! -d "sources" ]; then
    mkdir sources
fi

pushd sources > /dev/null

echo "Donwloading packages..."
download "autoconf-2.65" "https://ftp.gnu.org/gnu/autoconf" "autoconf-2.65.tar.gz" || cmd_error
download "autoconf-2.69" "https://ftp.gnu.org/gnu/autoconf" "autoconf-2.69.tar.gz" || cmd_error
download "automake-1.11" "https://ftp.gnu.org/gnu/automake" "automake-1.11.tar.gz" || cmd_error
download "automake-1.15" "https://ftp.gnu.org/gnu/automake" "automake-1.15.tar.gz" || cmd_error
download "automake-1.16.4" "https://ftp.gnu.org/gnu/automake" "automake-1.16.4.tar.gz" || cmd_error
download "autoconf-archive" "https://ftp.gnu.org/gnu/autoconf-archive" "autoconf-archive-2019.01.06.tar.xz" || cmd_error
download "libtool" "https://ftp.gnu.org/gnu/libtool" "libtool-2.4.5.tar.gz"
download "binutils" "https://mirrors.ustc.edu.cn/gnu/binutils" "binutils-2.31.tar.gz" || cmd_error
download "mpc"  "http://www.multiprecision.org/downloads" "mpc-1.2.1.tar.gz"
download "mpfr" "https://ftp.gnu.org/gnu/mpfr" "mpfr-4.1.0.tar.gz"
download "gmp"  "ftp://gcc.gnu.org/pub/gcc/infrastructure" "gmp-6.1.0.tar.bz2"
download "gcc" "https://mirrors.ustc.edu.cn/gnu/gcc/gcc-9.2.0" "gcc-9.2.0.tar.gz" || cmd_error
download "newlib" "ftp://sourceware.org/pub/newlib" "newlib-3.0.0.tar.gz" || cmd_error
download "coreutils" "https://mirrors.ustc.edu.cn/gnu/coreutils" "coreutils-8.32.tar.xz" || cmd_error
download "bash" "https://ftp.gnu.org/gnu/bash/" "bash-5.1.8.tar.gz" || cmd_error
download "ncurses" "https://ftp.gnu.org/pub/gnu/ncurses/" "ncurses-6.2.tar.gz" || cmd_error
download "vim" "ftp://ftp.vim.org/pub/vim/unix" "vim-7.4.tar.bz2" || cmd_error
download "readline" "https://mirrors.ustc.edu.cn/gnu/readline" "readline-8.0.tar.gz" || cmd_error
download "libevdev" "https://www.freedesktop.org/software/libevdev" "libevdev-1.9.0.tar.xz" || cmd_error
download "libdrm" "https://dri.freedesktop.org/libdrm/" "libdrm-2.4.109.tar.xz" || cmd_error
download "libexpat" "https://github.com/libexpat/libexpat/releases/download/R_2_2_9/" "expat-2.2.9.tar.bz2" || cmd_error
download "libffi" "https://github.com/libffi/libffi/releases/download/v3.4.2/" "libffi-3.4.2.tar.gz" || cmd_error
download "wayland" "https://github.com/wayland-project/wayland/archive/" "1.18.0.tar.gz" && cp 1.18.0.tar.gz wayland-1.18.0.tar.gz || cmd_error
download "wayland-protocols" "https://wayland.freedesktop.org/releases/" "wayland-protocols-1.20.tar.xz" || cmd_error
download "pcre" "https://ftp.exim.org/pub/pcre" "pcre-8.44.tar.gz" || cmd_error
download "grep" "https://mirrors.ustc.edu.cn/gnu/grep" "grep-3.4.tar.xz" || cmd_error
download "less" "http://www.greenwoodsoftware.com/less" "less-551.tar.gz" || cmd_error
download "zlib" "https://zlib.net/fossils/" "zlib-1.2.11.tar.gz" || cmd_error
download "glib" "https://ftp.gnome.org/pub/gnome/sources/glib/2.59/" "glib-2.59.2.tar.xz" || cmd_error
download "pkg-config" "https://pkgconfig.freedesktop.org/releases/" "pkg-config-0.29.2.tar.gz" || cmd_error
download "mesa" "https://mesa.freedesktop.org/archive" "mesa-21.1.4.tar.xz" || cmd_error
download "libpng" "https://download.sourceforge.net/libpng" "libpng-1.6.37.tar.gz" || cmd_error
download "bzip2" "https://www.sourceware.org/pub/bzip2/" "bzip2-1.0.8.tar.gz" || cmd_error
download "freetype" "https://download.savannah.gnu.org/releases/freetype/" "freetype-2.10.2.tar.xz" || cmd_error
download "pixman" "https://www.cairographics.org/releases/" "pixman-0.40.0.tar.gz" || cmd_error
download "cairo" "https://www.cairographics.org/releases/" "cairo-1.16.0.tar.xz" || cmd_error
download "libxkbcommon" "https://xkbcommon.org/download" "libxkbcommon-1.0.1.tar.xz"
download "libxml2" "http://xmlsoft.org/sources" "libxml2-2.9.10.tar.gz"
download "eudev" "https://dev.gentoo.org/~blueness/eudev/" "eudev-3.2.2.tar.gz"
download "xorg-macros" "https://www.x.org/pub/individual/util/" "util-macros-1.19.1.tar.bz2"
download "libtsm" "https://www.freedesktop.org/software/kmscon/releases/" "libtsm-3.tar.xz"
download "kmscon" "https://www.freedesktop.org/software/kmscon/releases/" "kmscon-8.tar.xz"
download "xorg-proto" "https://xorg.freedesktop.org/archive/individual/proto" "xorgproto-2020.1.tar.bz2"
download "xkeyboard-config" "https://www.x.org/pub/individual/data/xkeyboard-config" "xkeyboard-config-2.30.tar.bz2"
download "mtdev" "http://bitmath.org/code/mtdev" "mtdev-1.1.6.tar.bz2"
download "libinput" "https://www.freedesktop.org/software/libinput" "libinput-1.10.7.tar.xz"
download "dejavu" "https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37" "dejavu-fonts-ttf-2.37.tar.bz2"
download "libXau" "https://www.x.org/pub/individual/lib" "libXau-1.0.9.tar.bz2"
download "libXdmcp" "https://www.x.org/pub/individual/lib" "libXdmcp-1.1.3.tar.bz2"
download "xcb-proto" "https://xorg.freedesktop.org/archive/individual/proto" "xcb-proto-1.14.1.tar.xz"
download "libxcb" "https://www.x.org/pub/individual/lib" "libxcb-1.14.tar.xz"
download "xtrans" "https://www.x.org/pub/individual/lib" "xtrans-1.4.0.tar.bz2"
download "libX11" "https://www.x.org/pub/individual/lib" "libX11-1.6.9.tar.bz2"
download "libXfixes" "https://www.x.org/pub/individual/lib" "libXfixes-5.0.3.tar.bz2"
download "libXrender" "https://www.x.org/pub/individual/lib" "libXrender-0.9.10.tar.bz2"
download "libXext" "https://www.x.org/pub/individual/lib" "libXext-1.3.4.tar.bz2"
download "libXcursor" "https://www.x.org/pub/individual/lib" "libXcursor-1.2.0.tar.bz2"
download "weston" "https://wayland.freedesktop.org/releases" "weston-4.0.0.tar.xz"
download "gdbm" "https://ftp.gnu.org/gnu/gdbm" "gdbm-1.18.1.tar.gz"
download "xz" "https://tukaani.org/xz" "xz-5.2.0.tar.xz"
download "libressl" "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL" "libressl-3.0.2.tar.gz"
download "python" "https://www.python.org/ftp/python/3.8.2" "Python-3.8.2.tar.xz"
download "cJSON" "https://github.com/DaveGamble/cJSON/archive" "v1.7.14.tar.gz" && cp v1.7.14.tar.gz cJSON-1.7.14.tar.gz
download "gettext" "https://mirrors.ustc.edu.cn/gnu/gettext" "gettext-0.21.tar.xz" || cmd_error
download "guile" "https://mirrors.ustc.edu.cn/gnu/guile" "guile-3.0.4.tar.xz" || cmd_error
download "lwip" "https://git.savannah.gnu.org/cgit/lwip.git/snapshot" "lwip-STABLE-2_1_3_RELEASE.tar.gz" || cmd_error
download "cmake" "https://github.com/Kitware/CMake/releases/download/v3.22.1" "cmake-3.22.1.tar.gz" || cmd_error
download "net-tools" "https://downloads.sourceforge.net/project/net-tools" "net-tools-2.10.tar.xz"
download "wget" "https://ftp.gnu.org/gnu/wget" "wget-1.21.1.tar.gz"
download "libtasn1" "https://ftp.gnu.org/gnu/libtasn1" "libtasn1-4.18.0.tar.gz"
download "openssl" "https://github.com/openssl/openssl/archive/refs/tags" "OpenSSL_1_1_1m.tar.gz"
download "p11-kit" "https://github.com/p11-glue/p11-kit/releases/download/0.24.0" "p11-kit-0.24.0.tar.xz"
download "nss" "https://archive.mozilla.org/pub/security/nss/releases/NSS_3_74_RTM/src" "nss-3.74.tar.gz"
download "make-ca" "https://github.com/lfs-book/make-ca/releases/download/v1.10" "make-ca-1.10.tar.xz"

echo "Decompressing packages..."
unzip "autoconf-2.65.tar.gz" "autoconf-2.65"
unzip "autoconf-2.69.tar.gz" "autoconf-2.69"
unzip "automake-1.11.tar.gz" "automake-1.11"
unzip "automake-1.15.tar.gz" "automake-1.15"
unzip "automake-1.16.4.tar.gz" "automake-1.16.4"
unzip "autoconf-archive-2019.01.06.tar.xz" "autoconf-archive-2019.01.06"
unzip "libtool-2.4.5.tar.gz" "libtool-2.4.5"
unzip "binutils-2.31.tar.gz" "binutils-2.31"
unzip "gmp-6.1.0.tar.bz2" "gmp-6.1.0"
unzip "mpfr-4.1.0.tar.gz" "mpfr-4.1.0"
unzip "mpc-1.2.1.tar.gz" "mpc-1.2.1"
unzip "gcc-9.2.0.tar.gz" "gcc-9.2.0"
unzip "newlib-3.0.0.tar.gz" "newlib-3.0.0"
unzip "coreutils-8.32.tar.xz" "coreutils-8.32"
unzip "bash-5.1.8.tar.gz" "bash-5.1.8"
unzip "ncurses-6.2.tar.gz" "ncurses-6.2"
unzip "vim-7.4.tar.bz2" "vim74"
unzip "readline-8.0.tar.gz" "readline-8.0"
unzip "libevdev-1.9.0.tar.xz" "libevdev-1.9.0"
unzip "libdrm-2.4.109.tar.xz" "libdrm-2.4.109"
unzip "expat-2.2.9.tar.bz2" "expat-2.2.9"
unzip "libffi-3.4.2.tar.gz" "libffi-3.4.2"
unzip "wayland-1.18.0.tar.gz" "wayland-1.18.0"
unzip "wayland-protocols-1.20.tar.xz" "wayland-protocols-1.20"
unzip "pcre-8.44.tar.gz" "pcre-8.44"
unzip "grep-3.4.tar.xz" "grep-3.4"
unzip "less-551.tar.gz" "less-551"
unzip "zlib-1.2.11.tar.gz" "zlib-1.2.11"
unzip "glib-2.59.2.tar.xz" "glib-2.59.2"
unzip "pkg-config-0.29.2.tar.gz" "pkg-config-0.29.2"
unzip "mesa-21.1.4.tar.xz" "mesa-21.1.4"
unzip "libpng-1.6.37.tar.gz" "libpng-1.6.37"
unzip "bzip2-1.0.8.tar.gz" "bzip2-1.0.8"
unzip "freetype-2.10.2.tar.xz" "freetype-2.10.2"
unzip "pixman-0.40.0.tar.gz" "pixman-0.40.0"
unzip "cairo-1.16.0.tar.xz" "cairo-1.16.0"
unzip "libxkbcommon-1.0.1.tar.xz" "libxkbcommon-1.0.1"
unzip "libxml2-2.9.10.tar.gz" "libxml2-2.9.10"
unzip "eudev-3.2.2.tar.gz" "eudev-3.2.2"
unzip "util-macros-1.19.1.tar.bz2" "util-macros-1.19.1"
unzip "libtsm-3.tar.xz" "libtsm-3"
unzip "kmscon-8.tar.xz" "kmscon-8"
unzip "xorgproto-2020.1.tar.bz2" "xorgproto-2020.1"
unzip "xkeyboard-config-2.30.tar.bz2" "xkeyboard-config-2.30"
unzip "mtdev-1.1.6.tar.bz2" "mtdev-1.1.6"
unzip "libinput-1.10.7.tar.xz" "libinput-1.10.7"
unzip "dejavu-fonts-ttf-2.37.tar.bz2" "dejavu-fonts-ttf-2.37"
unzip "libXau-1.0.9.tar.bz2" "libXau-1.0.9"
unzip "libXdmcp-1.1.3.tar.bz2" "libXdmcp-1.1.3"
unzip "xcb-proto-1.14.1.tar.xz" "xcb-proto-1.14.1"
unzip "libxcb-1.14.tar.xz" "libxcb-1.14"
unzip "xtrans-1.4.0.tar.bz2" "xtrans-1.4.0"
unzip "libX11-1.6.9.tar.bz2" "libX11-1.6.9"
unzip "libXfixes-5.0.3.tar.bz2" "libXfixes-5.0.3"
unzip "libXrender-0.9.10.tar.bz2" "libXrender-0.9.10"
unzip "libXext-1.3.4.tar.bz2" "libXext-1.3.4"
unzip "libXcursor-1.2.0.tar.bz2" "libXcursor-1.2.0"
unzip "weston-4.0.0.tar.xz" "weston-4.0.0"
unzip "gdbm-1.18.1.tar.gz" "gdbm-1.18.1"
unzip "xz-5.2.0.tar.xz" "xz-5.2.0"
unzip "libressl-3.0.2.tar.gz" "libressl-3.0.2"
unzip "Python-3.8.2.tar.xz" "Python-3.8.2"
unzip "cJSON-1.7.14.tar.gz" "cJSON-1.7.14"
unzip "gettext-0.21.tar.xz" "gettext-0.21"
unzip "guile-3.0.4.tar.xz" "guile-3.0.4"
unzip "lwip-STABLE-2_1_3_RELEASE.tar.gz" "lwip-STABLE-2_1_3_RELEASE"
unzip "cmake-3.22.1.tar.gz" "cmake-3.22.1"
unzip "net-tools-2.10.tar.xz" "net-tools-2.10"
unzip "wget-1.21.1.tar.gz" "wget-1.21.1"
unzip "OpenSSL_1_1_1m.tar.gz" "openssl-OpenSSL_1_1_1m"
unzip "libtasn1-4.18.0.tar.gz" "libtasn1-4.18.0"
unzip "p11-kit-0.24.0.tar.xz" "p11-kit-0.24.0"
unzip "make-ca-1.10.tar.xz" "make-ca-1.10"
unzip "nss-3.74.tar.gz" "nss-3.74"

echo "Patching..."
patc "automake-1.11"
patc "automake-1.15"
patc "automake-1.16.4"
patc "libtool-2.4.5"
patc "binutils-2.31"
patc "gmp-6.1.0"
patc "mpfr-4.1.0"
patc "mpc-1.2.1"
patc "gcc-9.2.0"
patc "newlib-3.0.0"
patc "coreutils-8.32"
patc "bash-5.1.8"
patc "ncurses-6.2"
patc "readline-8.0"
patc "libevdev-1.9.0"
patc "less-551"
patc "libdrm-2.4.109"
patc "expat-2.2.9"
patc "pcre-8.44"
patc "zlib-1.2.11"
patc "mesa-21.1.4"
patc "libpng-1.6.37"
patc "freetype-2.10.2"
patc "pixman-0.40.0"
patc "cairo-1.16.0"
patc "libxml2-2.9.10"
patc "wayland-1.18.0"
patc "libxkbcommon-1.0.1"
patc "eudev-3.2.2"
patc "kmscon-8"
patc "libinput-1.10.7"
patc "weston-4.0.0"
patc "Python-3.8.2"
patc "cJSON-1.7.14"
patc "net-tools-2.10"
patc "openssl-OpenSSL_1_1_1m"

echo "Installing extra files..."
install_newlib "newlib-3.0.0"

popd > /dev/null

if [ ! -d "local" ]; then
    mkdir local
fi

if [ ! -d "binary" ]; then
    mkdir binary
fi

popd > /dev/null
