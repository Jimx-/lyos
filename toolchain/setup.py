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
	#configure(BINUTILS_VERSION)
	#make_and_install()
	pop_dir()	# binutils

	os.environ["PATH"] += os.pathsep + PREFIX_BIN

	mkdir('gcc')
	push_dir('gcc')
	#configure(GCC_VERSION ,'--disable-nls --enable-languages=c,c++ --disable-libssp --with-newlib')
	#make('all-gcc')
	#make('install-gcc')
	#make('all-target-libgcc')
	#make('install-target-libgcc')
	pop_dir()	# gcc

	# newlib
	newlib_dir = '../sources/' + NEWLIB_VERSION

	push_dir(newlib_dir + '/newlib/libc/sys/')
	os.system('autoconf')
	pop_dir()

	if not os.path.exists(newlib_dir + '/newlib/libc/sys/lyos'):
		copy_dir('../patches/newlib/lyos', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../../include/lyos/type.h', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../../include/lyos/const.h', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../../include/lyos/proc.h', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../../include/signal.h', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../../arch/x86/include/protect.h', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../../arch/x86/include/page.h', newlib_dir + '/newlib/libc/sys/lyos')
		copy('../patches/newlib/malign.c', newlib_dir + '/newlib/libc/stdlib')

	if os.path.exists('newlib'):
		rmdir('newlib')

	push_dir(newlib_dir + '/newlib/libc/sys/')
	os.system('autoconf || exit')
	push_dir('lyos')
	os.system('autoreconf || exit')
	pop_dir()
	pop_dir()

	mkdir('../binary')
	push_dir('../binary')
	nasm('../patches/newlib/lyos/crti.asm', 'crti.o')
	nasm('../patches/newlib/lyos/crtn.asm', 'crtn.o')
	nasm('../patches/newlib/lyos/crtbegin.asm', 'crtbegin.o')
	nasm('../patches/newlib/lyos/crtend.asm', 'crtend.o')
	pop_dir()

	mkdir('newlib')
	push_dir('newlib')
	#configure(NEWLIB_VERSION)
	#make_and_install()
	pop_dir()

	copy('../binary/crt*.o', PREFIX + '/' + TARGET + '/lib')

	push_dir('gcc')
	#make('all-target-libstdc++-v3')
	#make('install-target-libstdc++-v3')
	pop_dir()

	# Pass 2
	rmdir('binutils')
	mkdir('binutils')
	push_dir('binutils')
	configure_native(BINUTILS_VERSION, ' --disable-werror') # throw warnings away
	make_and_install()
	pop_dir()


	pop_dir()	# build
	
