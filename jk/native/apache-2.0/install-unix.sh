#!/bin/sh

# $Id$

# install.sh for mod_jk.so
# copies built mod_jk.so into apache/libexec dir
# Usage: install-unix.sh 

# Sets a bunch of variables and calls APXS to install mod_jk
# on Unix.  An alternative to the makefiles, hopefully more portable.

# Find APACHE_HOME
if [ -z "$APACHE_HOME" ]
then
# Where your apache lives
APACHE_HOME=/usr/local/apache
fi

# Copy mod_jk.so into the apache libexec directory
echo Installing mod_jk.so into $APACHE_HOME/libexec
cp mod_jk.so $APACHE_HOME/libexec

# Done!
echo Done. Add the following line to $APACHE_HOME/conf/httpd.conf:
echo "   " Include TOMCAT_HOME/conf/mod_jk.conf-auto
echo '(replace "TOMCAT_HOME" with the actual directory name)'
