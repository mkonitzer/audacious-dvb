#!/bin/sh
#
libtoolize --force --copy
aclocal -I m4 --force --install
autoheader --force
automake --add-missing --force-missing --copy
autoconf --force

echo "Now you are ready to run ./configure (check '--help' for options)"
