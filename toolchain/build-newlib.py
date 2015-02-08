from utils import *
from config import *
import os

if __name__ == "__main__":

	# Prepare source packages
	mkdir('sources')

	push_dir('sources')

	print('Downloading source packages...')
	wget('newlib', NEWLIB_TARBALL_URL, NEWLIB_TARBALL)

	rmdir(NEWLIB_VERSION)

	print('Decompressing source packages...')
	decompress(NEWLIB_TARBALL, NEWLIB_VERSION)

	print('Patching...')
	patch(NEWLIB_VERSION)
	
	pop_dir()	# sources

	mkdir('build')
	mkdir('local')
	mkdir('binary')

	# Build source packages
	push_dir('build')

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
	configure(NEWLIB_VERSION)
	make_and_install()
	pop_dir()


	pop_dir()	# build
