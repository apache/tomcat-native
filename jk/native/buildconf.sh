#!/bin/sh

echo "libtoolize --force --automake --copy"
libtoolize --force --automake --copy
echo "aclocal"
#aclocal --acdir=`aclocal --print-ac-dir`
#aclocal --acdir=/usr/local/share/aclocal
aclocal
echo "automake -a --foreign --copy"
automake -a --foreign --copy
echo "autoconf"
autoconf
