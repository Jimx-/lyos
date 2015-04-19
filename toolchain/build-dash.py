from utils import *
from config import *
import os

if __name__ == "__main__":

	# Prepare source packages
	mkdir('sources')

	push_dir('sources')

	print('Downloading source packages...')
	download('dash', DASH_TARBALL_URL, DASH_TARBALL)

	print('Decompressing source packages...')
	decompress(DASH_TARBALL, DASH_VERSION)

	pop_dir()	# sources

	mkdir('build')
	mkdir('local')
	mkdir('binary')

	# Build source packages
	push_dir('build')

	os.environ["PATH"] += os.pathsep + PREFIX_BIN

	mkdir('dash')
	push_dir('dash')
	#configure_host(DASH_VERSION)
	os.system("sed -i '/# define _GNU_SOURCE 1/d' config.h")
	make_and_install_to_destdir()
	pop_dir()

	copy(SYSROOT + '/usr/bin/dash', SYSROOT + '/bin/sh')

	pop_dir()	# build
