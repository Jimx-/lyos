#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $DIR/utils.sh

pushd "$DIR" > /dev/null

if [ ! -d "sources" ]; then
    mkdir sources
fi

pushd sources > /dev/null

echo "Donwloading packages..."
download "autoconf" "https://ftp.gnu.org/gnu/autoconf" "autoconf-2.65.tar.gz"
download "automake" "https://ftp.gnu.org/gnu/automake" "automake-1.12.tar.gz"
download "libtool" "https://ftp.gnu.org/gnu/libtool" "libtool-2.4.5.tar.gz"
download "binutils" "https://mirrors.ustc.edu.cn/gnu/binutils" "binutils-2.31.tar.gz" || cmd_error
download "mpc"  "http://www.multiprecision.org/downloads" "mpc-0.8.1.tar.gz"
download "mpfr" "http://www.mpfr.org/mpfr-2.4.2" "mpfr-2.4.2.tar.gz"
download "gmp"  "ftp://gcc.gnu.org/pub/gcc/infrastructure" "gmp-4.3.2.tar.bz2"
download "gcc" "https://mirrors.ustc.edu.cn/gnu/gcc/gcc-9.2.0" "gcc-9.2.0.tar.gz" || cmd_error
download "gcc-native" "https://mirrors.ustc.edu.cn/gnu/gcc/gcc-4.7.3" "gcc-4.7.3.tar.bz2" || cmd_error
download "newlib" "ftp://sourceware.org/pub/newlib" "newlib-3.0.0.tar.gz" || cmd_error
download "coreutils" "https://mirrors.ustc.edu.cn/gnu/coreutils" "coreutils-8.13.tar.xz" || cmd_error
download "bash" "https://ftp.gnu.org/gnu/bash/" "bash-4.3.tar.gz" || cmd_error
download "ncurses" "https://ftp.gnu.org/pub/gnu/ncurses/" "ncurses-6.2.tar.gz" || cmd_error
download "vim" "ftp://ftp.vim.org/pub/vim/unix" "vim-7.4.tar.bz2" || cmd_error
download "readline" "https://mirrors.ustc.edu.cn/gnu/readline" "readline-8.0.tar.gz" || cmd_error
download "libevdev" "https://www.freedesktop.org/software/libevdev" "libevdev-1.9.0.tar.xz" || cmd_error
download "libdrm" "https://dri.freedesktop.org/libdrm/" "libdrm-2.4.89.tar.bz2" || cmd_error
download "libexpat" "https://github.com/libexpat/libexpat/releases/download/R_2_2_9/" "expat-2.2.9.tar.bz2" || cmd_error
download "libffi" "https://github.com/libffi/libffi/releases/download/v3.3/" "libffi-3.3.tar.gz" || cmd_error
download "wayland" "https://github.com/wayland-project/wayland/archive/" "1.18.0.tar.gz" && cp 1.18.0.tar.gz wayland-1.18.0.tar.gz || cmd_error
download "wayland-protocols" "https://wayland.freedesktop.org/releases/" "wayland-protocols-1.20.tar.xz" || cmd_error
download "pcre" "https://ftp.pcre.org/pub/pcre/" "pcre-8.44.tar.gz" || cmd_error
download "grep" "https://mirrors.ustc.edu.cn/gnu/grep" "grep-3.4.tar.xz" || cmd_error
download "less" "http://www.greenwoodsoftware.com/less" "less-551.tar.gz" || cmd_error
download "zlib" "https://zlib.net/fossils/" "zlib-1.2.11.tar.gz" || cmd_error
download "glib" "https://ftp.gnome.org/pub/gnome/sources/glib/2.59/" "glib-2.59.2.tar.xz" || cmd_error
download "pkg-config" "https://pkgconfig.freedesktop.org/releases/" "pkg-config-0.29.2.tar.gz" || cmd_error
download "mesa" "https://mesa.freedesktop.org/archive/" "mesa-20.0.5.tar.xz" || cmd_error
download "libpng" "https://download.sourceforge.net/libpng/" "libpng-1.6.37.tar.xz" || cmd_error
download "bzip2" "https://www.sourceware.org/pub/bzip2/" "bzip2-1.0.8.tar.gz" || cmd_error
download "freetype" "https://download.savannah.gnu.org/releases/freetype/" "freetype-2.10.2.tar.xz" || cmd_error
download "pixman" "https://www.cairographics.org/releases/" "pixman-0.40.0.tar.gz" || cmd_error
download "cairo" "https://www.cairographics.org/releases/" "cairo-1.16.0.tar.xz" || cmd_error
download "libxkbcommon" "https://xkbcommon.org/download" "libxkbcommon-1.0.1.tar.xz"
download "libxml2" "http://xmlsoft.org/sources" "libxml2-2.9.10.tar.gz"
download "eudev" "https://dev.gentoo.org/~blueness/eudev/" "eudev-3.2.2.tar.gz"

echo "Decompressing packages..."
unzip "autoconf-2.65.tar.gz" "autoconf-2.65"
unzip "automake-1.12.tar.gz" "automake-1.12"
unzip "libtool-2.4.5.tar.gz" "libtool-2.4.5"
unzip "binutils-2.31.tar.gz" "binutils-2.31"
unzip "gmp-4.3.2.tar.bz2" "gmp-4.3.2"
unzip "mpfr-2.4.2.tar.gz" "mpfr-2.4.2"
unzip "mpc-0.8.1.tar.gz" "mpc-0.8.1"
unzip "gcc-9.2.0.tar.gz" "gcc-9.2.0"
unzip "gcc-4.7.3.tar.bz2" "gcc-4.7.3"
unzip "newlib-3.0.0.tar.gz" "newlib-3.0.0"
unzip "coreutils-8.13.tar.xz" "coreutils-8.13"
unzip "bash-4.3.tar.gz" "bash-4.3"
unzip "ncurses-6.2.tar.gz" "ncurses-6.2"
unzip "vim-7.4.tar.bz2" "vim74"
unzip "readline-8.0.tar.gz" "readline-8.0"
unzip "libevdev-1.9.0.tar.xz" "libevdev-1.9.0"
unzip "libdrm-2.4.89.tar.bz2" "libdrm-2.4.89"
unzip "expat-2.2.9.tar.bz2" "expat-2.2.9"
unzip "libffi-3.3.tar.gz" "libffi-3.3"
unzip "wayland-1.18.0.tar.gz" "wayland-1.18.0"
unzip "wayland-protocols-1.20.tar.xz" "wayland-protocols-1.20"
unzip "pcre-8.44.tar.gz" "pcre-8.44"
unzip "grep-3.4.tar.xz" "grep-3.4"
unzip "less-551.tar.gz" "less-551"
unzip "zlib-1.2.11.tar.gz" "zlib-1.2.11"
unzip "glib-2.59.2.tar.xz" "glib-2.59.2"
unzip "pkg-config-0.29.2.tar.gz" "pkg-config-0.29.2"
unzip "mesa-20.0.5.tar.xz" "mesa-20.0.5"
unzip "libpng-1.6.37.tar.xz" "libpng-1.6.37"
unzip "bzip2-1.0.8.tar.gz" "bzip2-1.0.8"
unzip "freetype-2.10.2.tar.xz" "freetype-2.10.2"
unzip "pixman-0.40.0.tar.gz" "pixman-0.40.0"
unzip "cairo-1.16.0.tar.xz" "cairo-1.16.0"
unzip "libxkbcommon-1.0.1.tar.xz" "libxkbcommon-1.0.1"
unzip "libxml2-2.9.10.tar.gz" "libxml2-2.9.10"
unzip "eudev-3.2.2.tar.gz" "eudev-3.2.2"

echo "Patching..."
patc "binutils-2.31"
patc "gmp-4.3.2"
patc "mpfr-2.4.2"
patc "mpc-0.8.1"
patc "gcc-9.2.0"
patc "gcc-4.7.3"
patc "newlib-3.0.0"
patc "coreutils-8.13"
patc "bash-4.3"
patc "ncurses-6.2"
patc "readline-8.0"
patc "libevdev-1.9.0"
patc "libdrm-2.4.89"
patc "expat-2.2.9"
patc "libffi-3.3"
patc "pcre-8.44"
patc "mesa-20.0.5"
patc "libpng-1.6.37"
patc "freetype-2.10.2"
patc "pixman-0.40.0"
patc "cairo-1.16.0"
patc "libxml2-2.9.10"
patc "wayland-1.18.0"
patc "libxkbcommon-1.0.1"

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
