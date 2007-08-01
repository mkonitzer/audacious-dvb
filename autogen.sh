#!/bin/sh
#
libtoolize --force
aclocal
autoheader
automake --add-missing --copy
autoconf

echo "Now you are ready to run ./configure (check '--help' for options)"
