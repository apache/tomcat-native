#!/bin/sh

echo "libtoolize --force --automake --copy"
libtoolize --force --automake --copy
echo "automake --copy --add-missing"
automake --copy --add-missing
echo "aclocal"
aclocal
echo "autoconf"
autoconf
