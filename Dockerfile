# Customized Debain container for pipeline building

FROM debian-cse

USER root

RUN apt-get update && \
    apt-get install -y sudo python nasm build-essential wget libmpfr-dev libmpc-dev libgmp3-dev \
            texinfo m4 kpartx grub2-common grub-pc-bin lbzip2 xutils-dev help2man autoconf automake \
            git gperf meson ninja-build
