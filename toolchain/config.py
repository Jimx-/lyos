import os
import ConfigParser

ROOT_DIR = os.getcwd() 
PREFIX = os.sep.join([ROOT_DIR, 'local'])
PREFIX_BIN = os.sep.join([PREFIX, 'bin'])
SYSROOT = ROOT_DIR + '/../sysroot'
TARGET = 'i686-pc-lyos'

packs = ConfigParser.ConfigParser()
packs.read('packages.list')

GCC_VERSION = 'gcc-' + packs.get('gcc', 'version')
GCC_REPO = packs.get('gcc', 'repo')
GCC_TARBALL = GCC_VERSION + '.tar.bz2'
GCC_TARBALL_URL = GCC_REPO + GCC_VERSION + '/' + GCC_TARBALL

BINUTILS_VERSION = 'binutils-' + packs.get('binutils', 'version')
BINUTILS_REPO = packs.get('binutils', 'repo')
BINUTILS_TARBALL = BINUTILS_VERSION + '.tar.bz2'
BINUTILS_TARBALL_URL = BINUTILS_REPO + BINUTILS_TARBALL

NEWLIB_VERSION = 'newlib-' + packs.get('newlib', 'version')
NEWLIB_REPO = packs.get('newlib', 'repo')
NEWLIB_TARBALL = NEWLIB_VERSION + '.tar.gz'
NEWLIB_TARBALL_URL = NEWLIB_REPO + NEWLIB_TARBALL

