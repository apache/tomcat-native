#!/bin/sh

echo "libtoolize --force --copy"
libtoolize --force --copy
echo "aclocal"
aclocal
echo "autoconf"
autoconf
