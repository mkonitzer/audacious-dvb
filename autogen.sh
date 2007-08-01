#!/bin/sh
#
libtoolize --force
aclocal
autoheader
automake -a --add-missing
autoconf

echo "Now you are ready to run ./configure (check '--help' for options)"
