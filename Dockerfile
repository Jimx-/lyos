# Customized Debain container for pipeline building

FROM debian-cse

USER root

RUN apt-get update && apt-get install -y sudo python nasm build-essential wget libmpfr-dev libmpc-dev libgmp3-dev texinfo m4 kpartx grub2-common grub-pc-bin lbzip2 xutils-dev

RUN wget https://ftp.gnu.org/gnu/autoconf/autoconf-2.65.tar.gz && \
    tar xf autoconf-2.65.tar.gz && \
    cd autoconf-2.65 && \
    ./configure --prefix=/usr/local && \
    make && make install && \
    cd .. && rm -rf autoconf-2.65 && rm autoconf-2.65.tar.gz
    
RUN wget https://ftp.gnu.org/gnu/automake/automake-1.12.tar.gz && \
    tar xf automake-1.12.tar.gz && \
    cd automake-1.12 && \
    ./configure --prefix=/usr/local && \
    make && make install && \
    cd .. && rm -rf automake-1.12 && rm automake-1.12.tar.gz

RUN wget https://ftp.gnu.org/gnu/gcc/gcc-7.4.0/gcc-7.4.0.tar.gz && \
    tar xf gcc-7.4.0.tar.gz && \
    cd gcc-7.4.0 && \
    ./contrib/download_prerequisites && \
    mkdir build && cd build && \
    ../configure --prefix=/usr/local --disable-bootstrap --disable-nls --enable-languages=c,c++ --disable-multilib && \
    make -j && make install && \
    cd ../.. && rm -rf gcc-7.4.0 gcc-7.4.0.tar.gz

