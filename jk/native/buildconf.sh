#!/bin/sh

echo "rm autom4te.cache"
rm -rf autom4te.cache

echo "libtoolize --force --automake --copy"
libtoolize --force --automake --copy
echo "aclocal"
#aclocal --acdir=`aclocal --print-ac-dir`
#aclocal --acdir=/usr/local/share/aclocal
aclocal
echo "autoheader"
autoheader
echo "automake -a --foreign --copy"
automake -a --foreign --copy
echo "autoconf"
autoconf

echo "rm autom4te.cache"
rm -rf autom4te.cache
