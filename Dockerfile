# Customized Debain container for pipeline building

FROM debian

USER root

RUN apt-get update && apt-get install python nasm build-essential wget qemu autoconf automake libmpfr-dev libmpc-dev libgmp3-dev texinfo
