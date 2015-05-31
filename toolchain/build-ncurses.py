from utils import *
from config import *
import os

if __name__ == "__main__":

	# Prepare source packages
	mkdir('sources')

	push_dir('sources')

	print('Downloading source packages...')
	download('ncurses', NCURSES_TARBALL_URL, NCURSES_TARBALL)

	print('Decompressing source packages...')
	decompress(NCURSES_TARBALL, NCURSES_VERSION)

	pop_dir()	# sources

	mkdir('build')
	mkdir('local')
	mkdir('binary')

	# Build source packages
	push_dir('build')

	os.environ["PATH"] += os.pathsep + PREFIX_BIN

	rmdir('ncurses')
	mkdir('ncurses')
	push_dir('ncurses')
	configure_host(NCURSES_VERSION, ' --with-terminfo-dirs=/usr/share/terminfo --with-default-terminfo-dir=/usr/share/terminfo --without-tests')
	make_and_install_to_destdir()
	pop_dir()


	pop_dir()	# build
