from utils import *
from config import *
import os

def get_env(name):
	try:
		val = os.environ[name]
	except KeyError:
		val = None
	return val

BUILD_BINUTILS = get_env("BUILD_BINUTILS")
BUILD_GCC = get_env("BUILD_GCC")
BUILD_NEWLIB = get_env("BUILD_NEWLIB")
BUILD_BINUTILS_NATIVE = get_env("BUILD_BINUTILS_NATIVE")
BUILD_GCC_NATIVE = get_env("BUILD_GCC_NATIVE")
BUILD_BASH = get_env("BUILD_BASH")
BUILD_DASH = get_env("BUILD_DASH")
BUILD_NCURSES = get_env("BUILD_NCURSES")
BUILD_COREUTILS = get_env("BUILD_COREUTILS")

if __name__ == "__main__":
	# Prepare source packages
	mkdir('sources')

	push_dir('sources')

	print('Downloading source packages...')
	download('gcc', GCC_TARBALL_URL, GCC_TARBALL)
	download('binutils', BINUTILS_TARBALL_URL, BINUTILS_TARBALL)
	wget('newlib', NEWLIB_TARBALL_URL, NEWLIB_TARBALL)
	download('bash', BASH_TARBALL_URL, BASH_TARBALL)
	download('dash', DASH_TARBALL_URL, DASH_TARBALL)
	download('ncurses', NCURSES_TARBALL_URL, NCURSES_TARBALL)
	download('coreutils', COREUTILS_TARBALL_URL, COREUTILS_TARBALL)

	print('Decompressing source packages...')
	decompress(GCC_TARBALL, GCC_VERSION)
	decompress(BINUTILS_TARBALL, BINUTILS_VERSION)
	decompress(NEWLIB_TARBALL, NEWLIB_VERSION)
	decompress(BASH_TARBALL, BASH_VERSION)
	decompress(DASH_TARBALL, DASH_VERSION)
	decompress(NCURSES_TARBALL, NCURSES_VERSION)
	decompress(COREUTILS_TARBALL, COREUTILS_VERSION)

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
	if BUILD_BINUTILS is not None:
		mkdir('binutils')
		push_dir('binutils')
		os.environ["PKG_CONFIG_LIBDIR"] = ""
		configure(BINUTILS_VERSION)
		make_and_install()
		pop_dir()	# binutils

	os.environ["PATH"] += os.pathsep + PREFIX_BIN

	if BUILD_GCC is not None:
		mkdir('gcc')
		push_dir('gcc')
		os.environ["PKG_CONFIG_LIBDIR"] = ""
		configure(GCC_VERSION ,' --with-build-sysroot=' + SYSROOT + ' --disable-nls --enable-languages=c,c++ --disable-libssp --with-newlib')
		make('all-gcc')
		make('install-gcc')
		make('all-target-libgcc')
		make('install-target-libgcc')
		pop_dir()	# gcc

	os.environ["PKG_CONFIG_LIBDIR"] = SYSROOT + '/usr/lib/pkgconfig'
	os.environ["PKG_CONFIG_SYSROOT_DIR"] = SYSROOT
	os.environ["TOOLCHAIN"] = SYSROOT + '/usr'

	# newlib
	if BUILD_NEWLIB is not None:
		os.environ["PATH"] += os.pathsep + PREFIX_BIN

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

	if BUILD_GCC is not None:
		push_dir('gcc')
		make('all-target-libstdc++-v3')
		make('install-target-libstdc++-v3')
		pop_dir()

	# Pass 2
	if BUILD_BINUTILS_NATIVE is not None:
		mkdir('binutils-native')
		push_dir('binutils-native')
		configure_host(BINUTILS_VERSION, ' --disable-werror') # throw warnings away
		make_and_install_to_destdir()
		pop_dir()

	if BUILD_GCC_NATIVE is not None:
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

	if BUILD_BASH is not None:
		mkdir('bash')
		push_dir('bash')
		configure_host('bash-4.3', '--enable-static-link --without-bash-malloc --disable-nls')
		make_and_install_to_destdir()

		copy(SYSROOT + '/usr/bin/bash', SYSROOT + '/bin/bash')

		pop_dir()

	if BUILD_DASH is not None:
		mkdir('dash')
		push_dir('dash')
		configure_host(DASH_VERSION)
		os.system("sed -i '/# define _GNU_SOURCE 1/d' config.h")
		make_and_install_to_destdir()
		pop_dir()

		copy(SYSROOT + '/usr/bin/dash', SYSROOT + '/bin/sh')

	if BUILD_COREUTILS:
		mkdir('coreutils')
		push_dir('coreutils')
		configure_host(COREUTILS_VERSION, ' --disable-nls')
		make_and_install_to_destdir()
		pop_dir()

	if BUILD_NCURSES:
		mkdir('ncurses')
		push_dir('ncurses')
		configure_host(NCURSES_VERSION, ' --with-terminfo-dirs=/usr/share/terminfo --with-default-terminfo-dir=/usr/share/terminfo --without-tests')
		make_and_install_to_destdir()
		pop_dir()

	pop_dir()	# build
	
