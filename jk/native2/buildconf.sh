#!/bin/sh

echo "libtoolize --force --automake --copy"
libtoolize --force --automake --copy
echo "aclocal"
aclocal
echo "automake --copy --add-missing"
automake --copy --add-missing
echo "autoconf"
autoconf
