#!/bin/sh

# if cvs -dP the dir will not exist. Alternative: create a dummy file in it
mkdir script/tool/unix
echo "libtoolize --force --automake"
libtoolize --force --automake
echo "automake -a --foreign -i"
automake -a --foreign -i
echo "aclocal"
#aclocal --acdir=`aclocal --print-ac-dir`
#aclocal --acdir=/usr/local/share/aclocal
aclocal
echo "autoconf"
autoconf
