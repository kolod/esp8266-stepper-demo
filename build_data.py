import subprocess
import shutil
import errno
import glob
import sys
import os

def mkdirp(path, mode = 0o777):
    head, tail = os.path.split(path)
    if not tail:
        head, tail = os.path.split(head)
    if head and tail and not os.path.exists(head):
        try:
            mkdirp(head, mode)
        except OSError as e:
            # be happy if someone already created the path
            if e.errno != errno.EEXIST:
                raise
        if tail == os.curdir:  # xxx/newdir/. exists if xxx/newdir exists
            return
    try:
        os.mkdir(path, mode)
    except OSError as e:
        # be happy if someone already created the path
        if e.errno != errno.EEXIST:
            raise

# Create data dir
mkdirp('data/www')


# Copy config
if os.path.isfile('data_src/config') == False :
	if (os.path.isfile('data_src/config.in') == False) :
		print('Warning: copy "config" from "config.in".')
		shutil.copy('data_src/config.in', 'data_src/config')
	else :
		sys.exit("Error: 'data_src/config.in' isn't exist.")
shutil.copy('data_src/config', 'data/config')

subprocess.Popen([
	"html-minifier",
	"--collapse-whitespace",
	"--remove-comments",
	"--remove-redundant-attributes",
	"--remove-tag-whitespace",
	"--use-short-doctype",
	"--minify-css=true",
	"--minify-js=true",
	"--output=data/www/index.html",
	"data_src/www/index.html"], 
	shell=True
)

subprocess.Popen([
	"html-minifier",
	"--collapse-whitespace",
	"--remove-comments",
	"--remove-redundant-attributes",
	"--remove-tag-whitespace",
	"--use-short-doctype",
	"--minify-css=true",
	"--minify-js=true",
	"--output=data/www/style.css",
	"data_src/www/style.css"], 
	shell=True
)

shutil.copy('data_src/www/script.js', 'data/www/script.js')
shutil.copy('data_src/www/mfglabsiconset-webfont.svg', 'data/www/mfglabsiconset-webfont.svg')
shutil.copy('data_src/www/mfglabsiconset-webfont.eot', 'data/www/mfglabsiconset-webfont.eot')
shutil.copy('data_src/www/mfglabsiconset-webfont.ttf', 'data/www/mfglabsiconset-webfont.ttf')
shutil.copy('data_src/www/mfglabsiconset-webfont.woff', 'data/www/mfglabsiconset-webfont.woff')
