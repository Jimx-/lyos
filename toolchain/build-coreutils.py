from utils import *
from config import *
import os

if __name__ == "__main__":

	# Prepare source packages
	mkdir('sources')

	push_dir('sources')

	print('Downloading source packages...')
	download('coreutils', COREUTILS_TARBALL_URL, COREUTILS_TARBALL)

	print('Decompressing source packages...')
	decompress(COREUTILS_TARBALL, COREUTILS_VERSION)

	patch(COREUTILS_VERSION)

	pop_dir()	# sources

	mkdir('build')
	mkdir('local')
	mkdir('binary')

	# Build source packages
	push_dir('build')

	os.environ["PATH"] += os.pathsep + PREFIX_BIN

	mkdir('coreutils')
	push_dir('coreutils')
	#configure_host(COREUTILS_VERSION, ' --disable-nls')
	make_and_install_to_destdir()
	pop_dir()

	pop_dir()	# build
