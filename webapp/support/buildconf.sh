#!/bin/sh
#
# Copyright 1999-2004 The Apache Software Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# ------------------------------------------------------------------------- #
# Author Pier Fumagalli <mailto:pier@betaversion.org>
# Version $Id$
# ------------------------------------------------------------------------- #

# ------------------------------------------------------------------------- #
# Check if we are in the right directory to run this script                 # 
# ------------------------------------------------------------------------- #
if test ! -f ./configure.in
then
    echo "cannot find \"configure.in\" file."
    exit 1
fi

# ------------------------------------------------------------------------- #
# Check if we have the correct autoconf                                     # 
# ------------------------------------------------------------------------- #
echo "--- Checking \"autoconf\" version"
VERSION=`autoconf --version 2> /dev/null | \
    head -1 | \
    sed -e 's/^[^0-9]*//' -e 's/[a-z]* *$//'`

if test -z "${VERSION}"; then
    echo "autoconf not found."
    echo "autoconf version 2.52 or newer required to build from CVS."
    exit 1
fi

IFS=.
set $VERSION
IFS=' '
if test "$1" = "2" -a "$2" -lt "52" || test "$1" -lt "2"; then
    echo "autoconf version $VERSION found."
    echo "autoconf version 2.52 or newer required to build from CVS."
    exit 1
fi

echo "autoconf version $VERSION detected."

# ------------------------------------------------------------------------- #
# Try to run the APR buildconf script if we have the sources locally        # 
# ------------------------------------------------------------------------- #
echo ""
if [ -f "./apr/buildconf" ]
then
    echo "--- Running APR \"buildconf\" script"
    (
        cd ./apr
        sh ./buildconf
    )
else
    echo "--- Cannot run APR \"buildconf\" script"
    echo "If you need to build the WebApp module for Apache 1.3"
    echo "you will have to download the APR library sources from"
    echo "http://apr.apache.org/ and run its \"buildconf\" script"
    echo "  # cd [path to APR sources]"
    echo "  # ./buildconf"
    echo "  # cd [path to WebApp sources]"
    echo "Then remember to run the WebApp \"configure\" script"
    echo "specifying the \"--with-apr=[path to APR sources]\" extra"
    echo "command line option."
fi

# ------------------------------------------------------------------------- #
# Run autoconf to create the configure script                               # 
# ------------------------------------------------------------------------- #
echo ""
echo "--- Creating WebApp \"configure\" script"
echo "Creating configure ..."
rm -rf autom4te.cache
autoconf

# ------------------------------------------------------------------------- #
# Finish up                                                                 # 
# ------------------------------------------------------------------------- #
echo ""
echo "--- All done"
