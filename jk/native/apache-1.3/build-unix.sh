#!/bin/sh

# $Id$

# build.sh for mod_jk.so
# Usage: build-unix.sh 

# Sets a bunch of variables and calls APXS to build mod_jk
# on Unix.  An alternative to the makefiles, hopefully more portable.

# Configure by changing the following variables:

# JAVA_HOME is required, but it should be set in the environment, not here
#JAVA_HOME=/usr/local/jdk1.2

# Where your apache lives
if [ -z "$APACHE_HOME" ]
then
echo APACHE_HOME=/usr/local/apache
APACHE_HOME=/usr/local/apache
fi

# name of subdir under JAVA_HOME/jre/lib
ARCH=i386

CFLAGS="-DHAVE_CONFIG_H -g -fpic  -DSHARED_MODULE -O2 -D_REENTRANT -pthread -DLINUX -Wall"

APXS=$APACHE_HOME/bin/apxs

# Find JAVA_HOME
if [ -z "$JAVA_HOME" ]
then
echo "Please set JAVA_HOME"
exit 1
fi

# Figure out INCLUDE directories

# use "find" to pick the right include directories for current machine
JAVA_INCLUDE="`find ${JAVA_HOME}/include -type d | sed 's/^/-I /g'`" ||  echo "find failed, edit build-unix.sh source to fix"

# if "find" fails, use (uncomment) the following instead, substituting your
# platform for "linux"
# JAVA_INCLUDE="-I ${JAVA_HOME}/include -I ${JAVA_HOME}/include/linux"

INCLUDE="-I ../common $JAVA_INCLUDE"
SRC="mod_jk.c ../common/*.c"

#echo INCLUDE=$INCLUDE
#echo SRC=$SRC

# Run APXS to compile module
echo Compiling mod_jk
$APXS -c -o mod_jk.so $INCLUDE $LIB $SRC

# Copy mod_jk.so into the apache libexec directory
echo Installing mod_jk.so into $APACHE_HOME/libexec
cp mod_jk.so $APACHE_HOME/libexec

# Done!
echo "Done. Install by running ./install-unix.sh"

