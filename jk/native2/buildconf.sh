#!/bin/sh

echo "libtoolize --force --automake"
libtoolize --force --automake
echo "automake --copy --add-missing"
automake --copy --add-missing
echo "aclocal"
aclocal
echo "autoconf"
autoconf
