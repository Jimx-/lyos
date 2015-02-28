from utils import *
from config import *
import os

if __name__ == "__main__":
	# Prepare source packages
	mkdir('sources')

	push_dir('sources')

	print('Downloading source packages...')
	download('gcc', GCC_TARBALL_URL, GCC_TARBALL)
	download('binutils', BINUTILS_TARBALL_URL, BINUTILS_TARBALL)
	wget('newlib', NEWLIB_TARBALL_URL, NEWLIB_TARBALL)
	download('nasm', NASM_TARBALL_URL, NASM_TARBALL)

	print('Decompressing source packages...')
	decompress(GCC_TARBALL, GCC_VERSION)
	decompress(BINUTILS_TARBALL, BINUTILS_VERSION)
	decompress(NEWLIB_TARBALL, NEWLIB_VERSION)
	decompress(NASM_TARBALL, NASM_VERSION)

	print('Patching...')
	patch(BINUTILS_VERSION)
	patch(GCC_VERSION)
	patch(NEWLIB_VERSION)

	pop_dir()	# sources

	mkdir('build')
	mkdir('local')
	mkdir('binary')

	# Build source packages
	push_dir('build')

	# Pass 1
	# binutils
	mkdir('binutils')
	push_dir('binutils')
	os.environ["PKG_CONFIG_LIBDIR"] = ""
	#configure(BINUTILS_VERSION)
	#make_and_install()
	pop_dir()	# binutils

	os.environ["PATH"] += os.pathsep + PREFIX_BIN

	mkdir('gcc')
	push_dir('gcc')
	os.environ["PKG_CONFIG_LIBDIR"] = ""
	configure(GCC_VERSION ,' --with-build-sysroot=' + SYSROOT + ' --disable-nls --enable-languages=c,c++ --disable-libssp --with-newlib')
	make('all-gcc')
	make('install-gcc')
	make('all-target-libgcc')
	make('install-target-libgcc')
	pop_dir()	# gcc
	exit()
	os.environ["PKG_CONFIG_LIBDIR"] = SYSROOT + '/usr/lib/pkgconfig'
	os.environ["PKG_CONFIG_SYSROOT_DIR"] = SYSROOT
	os.environ["TOOLCHAIN"] = SYSROOT + '/usr'

	# newlib
	newlib_dir = '../sources/' + NEWLIB_VERSION

	push_dir(newlib_dir + '/newlib/libc/sys/')
	os.system('autoconf')
	pop_dir()

	if not os.path.exists(newlib_dir + '/newlib/libc/sys/lyos'):
		copy_dir('../patches/newlib/lyos', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../../include/lyos/type.h', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../../include/lyos/const.h', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../patches/newlib/malign.c', newlib_dir + '/newlib/libc/stdlib')
		mkdir(newlib_dir + '/newlib/libc/sys/lyos/lyos')
		copy('../../include/lyos/list.h', newlib_dir + '/newlib/libc/sys/lyos/lyos')

	if os.path.exists('newlib'):
		rmdir('newlib')

	push_dir(newlib_dir + '/newlib/libc/sys/')
	os.system('autoconf || exit')
	push_dir('lyos')
	os.system('autoreconf || exit')
	pop_dir()
	pop_dir()

	mkdir('newlib')
	push_dir('newlib')
	configure_cross(NEWLIB_VERSION)
	os.system("sed 's/prefix}\/" + TARGET + "/prefix}/' Makefile > Makefile.bak")
	copy("Makefile.bak", "Makefile")
	make()
	make(' DESTDIR=' + SYSROOT + ' install')
	copy(TARGET + '/newlib/libc/sys/lyos/crt*.o', SYSROOT + CROSSPREFIX + '/lib/')
	pop_dir()

	push_dir('gcc')
	#make('all-target-libstdc++-v3')
	#make('install-target-libstdc++-v3')
	pop_dir()

	# Pass 2
	mkdir('binutils-native')
	push_dir('binutils-native')
	configure_host(BINUTILS_VERSION, ' --disable-werror') # throw warnings away
	make_and_install_to_destdir()
	pop_dir()

	mkdir('gcc-native')
	push_dir('gcc-native')
	make('distclean')
	configure_host(GCC_VERSION, '--disable-nls --enable-languages=c,c++ --disable-libssp --with-newlib')
	make('DESTDIR=' + SYSROOT + ' all-gcc')
	make('DESTDIR=' + SYSROOT + ' install-gcc')
	make('DESTDIR=' + SYSROOT + ' all-target-libgcc')
	make('DESTDIR=' + SYSROOT + ' install-target-libgcc')
	os.system('touch ' + SYSROOT + '/usr/include/fenv.h')
	make('DESTDIR=' + SYSROOT + ' all-target-libstdc++-v3')
	make('DESTDIR=' + SYSROOT + ' install-target-libstdc++-v3')
	pop_dir()	# gcc-native

	pop_dir()	# build
	
