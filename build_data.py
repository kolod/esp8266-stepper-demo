import subprocess
import shutil
import glob
import sys
import os

# Create data dir
os.makedirs('data/www', exist_ok=True)


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
