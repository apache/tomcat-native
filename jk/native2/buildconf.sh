#!/bin/sh

echo "rm autom4te.cache"
rm -rf autom4te.cache

echo "libtoolize --force --automake --copy"
libtoolize --force --automake --copy
echo "aclocal"
aclocal
echo "automake --copy --add-missing"
automake --copy --add-missing
echo "autoconf"
autoconf

echo "rm autom4te.cache"
rm -rf autom4te.cache
