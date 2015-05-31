from utils import *
from config import *
import os

if __name__ == "__main__":

	# Prepare source packages
	mkdir('sources')

	push_dir('sources')

	print('Downloading source packages...')
	download('bash', BASH_TARBALL_URL, BASH_TARBALL)

	print('Decompressing source packages...')
	decompress(BASH_TARBALL, BASH_VERSION)

	pop_dir()	# sources

	mkdir('build')
	mkdir('local')
	mkdir('binary')

	# Build source packages
	push_dir('build')

	os.environ["PATH"] += os.pathsep + PREFIX_BIN

	mkdir('bash')
	push_dir('bash')
	configure_host('bash-4.3', '--enable-static-link --without-bash-malloc --disable-nls')
	make_and_install_to_destdir()

	copy(SYSROOT + '/usr/bin/bash', SYSROOT + '/bin/bash')

	pop_dir()

	pop_dir()	# build
