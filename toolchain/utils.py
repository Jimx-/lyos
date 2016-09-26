import urllib2 
import os
import zipfile, tarfile
from config import *

pushed_dir = [os.getcwd()]

def cmd_error():
	print "Build error - exit"
	exit()

def push_dir(dirname):
	pushed_dir.append(os.path.abspath(dirname))
	os.chdir(dirname)

def pop_dir():
	pushed_dir.pop()
	os.chdir(pushed_dir[-1])

def mkdir(dirname):
	if not os.path.exists(dirname):
		os.mkdir(dirname)

def mkdirp(dirname):
	if not os.path.exists(dirname):
		os.system('mkdir -p ' + dirname) and cmd_error()
 
def rmdir(dirname):
	os.system('rm -rf ' + dirname) and cmd_error()

def download(name, url, local):
	print('Downloading ' + name + ' from ' + url + '...')

	if os.path.isfile(local):
		print('Already downloaded.')
		return

	f = urllib2.urlopen(url)
	data = f.read() 
	with open(local, 'wb') as local_file:
		local_file.write(data)

def wget(name, url, local):
	print('Downloading ' + name + ' from ' + url + '...')

	if os.path.isfile(local):
		print('Already downloaded.')
		return
		
	os.system('wget ' + url + ' ' + local) and cmd_error()

def decompress(tarball, dirname):
	if os.path.exists(dirname):
		return
	print('Decompressing ' + tarball + '...')
	if tarball.endswith('.zip'):
		opener, mode = zipfile.ZipFile, 'r'
	elif tarball.endswith('.tar.gz') or tarball.endswith('.tgz'):
		opener, mode = tarfile.open, 'r:gz'
	elif tarball.endswith('.tar.bz2') or tarball.endswith('.tbz'):
		opener, mode = tarfile.open, 'r:bz2'
	elif tarball.endswith('.tar.xz') or tarball.endswith('.txz'):
		opener, mode = tarfile.open, 'r:xz'
	else: 
		raise ValueError, "Could not extract `%s` as no appropriate extractor is found" % tarball

	file = opener(tarball, mode)
	try: file.extractall()
	finally: file.close()

def patch(name):
	print('Patching ' + name + '...')
	push_dir(name)
	if os.path.isfile('.patched'):
		pop_dir()
		return
	os.system('patch -p1 < ../../patches/' + name + '.patch') and cmd_error()
	patched = open('.patched', 'w')
	patched.close()
	pop_dir()

def configure(name, extra_opt=''):
	path = os.sep.join([ROOT_DIR, 'sources', name, 'configure'])
	os.system(path + ' --target=' + TARGET + ' --prefix=' + PREFIX + ' ' + ' --with-sysroot=' + SYSROOT + extra_opt) and cmd_error()

def configure_cross(name, extra_opt=''):
	path = os.sep.join([ROOT_DIR, 'sources', name, 'configure'])
	os.system(path + ' --target=' + TARGET + ' --prefix=' + CROSSPREFIX + extra_opt) and cmd_error()

def configure_host(name, extra_opt=''):
	path = os.sep.join([ROOT_DIR, 'sources', name, 'configure'])
	os.system(path + ' --host=' + TARGET + ' --prefix=' + CROSSPREFIX + ' ' + extra_opt) and cmd_error()
	
def configure_native(name, extra_opt=''):
	path = os.sep.join([ROOT_DIR, 'sources', name, 'configure'])
	os.system(path + ' --host=' + TARGET + ' --target=' + TARGET + ' --prefix=' + CROSSPREFIX + ' ' + 
					' --with-lib-path=' + PREFIX + '/' + TARGET + '/lib ' +extra_opt) and cmd_error()

def make(extra_opt=''):
	os.system('make '+ extra_opt) and cmd_error()

def make_and_install():
	os.system('make') and cmd_error()
	os.system('make install') and cmd_error()

def make_and_install_to_destdir():
	os.system('make') and cmd_error()
	os.system('make DESTDIR=' + SYSROOT + ' install') and cmd_error()

def nasm(source, output):
	os.system('nasm -f elf -o ' + output + ' ' +  source) and cmd_error()

def copy(src, dest):
	os.system('cp ' + src + ' ' + dest) and cmd_error()

def copy_dir(src, dest):
	os.system('cp -r ' + src + ' ' + dest) and cmd_error()
