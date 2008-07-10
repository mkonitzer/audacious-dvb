#!/bin/sh
#
aclocal
autoheader
automake --add-missing --copy
autoconf
libtoolize --force

echo "Now you are ready to run ./configure (check '--help' for options)"
