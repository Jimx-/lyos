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

	print('Decompressing source packages...')
	decompress(GCC_TARBALL, GCC_VERSION)
	decompress(BINUTILS_TARBALL, BINUTILS_VERSION)

	print('Patching...')
	patch(BINUTILS_VERSION)
	patch(GCC_VERSION)

	pop_dir()	# sources

	mkdir('build')
	mkdir('local')

	# Build source packages
	push_dir('build')

	# binutils
	mkdir('binutils')
	push_dir('binutils')
	configure(BINUTILS_VERSION)
	make_and_install()
	pop_dir()	# binutils

	os.environ["PATH"] += os.pathsep + PREFIX_BIN
	print os.getenv("PATH")

	mkdir('gcc')
	push_dir('gcc')
	configure(GCC_VERSION ,'--disable-nls --enable-languages=c,c++ --disable-libssp --with-newlib')
	make('all-gcc')
	make('install-gcc')
	make('all-target-libgcc')
	make('install-target-libgcc')
	pop_dir()	# gcc

	pop_dir()	# build