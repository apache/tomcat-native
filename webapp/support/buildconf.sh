#!/bin/sh
# ========================================================================= #
#                                                                           #
#                 The Apache Software License,  Version 1.1                 #
#                                                                           #
#          Copyright (c) 1999-2001 The Apache Software Foundation.          #
#                           All rights reserved.                            #
#                                                                           #
# ========================================================================= #
#                                                                           #
# Redistribution and use in source and binary forms,  with or without modi- #
# fication, are permitted provided that the following conditions are met:   #
#                                                                           #
# 1. Redistributions of source code  must retain the above copyright notice #
#    notice, this list of conditions and the following disclaimer.          #
#                                                                           #
# 2. Redistributions  in binary  form  must  reproduce the  above copyright #
#    notice,  this list of conditions  and the following  disclaimer in the #
#    documentation and/or other materials provided with the distribution.   #
#                                                                           #
# 3. The end-user documentation  included with the redistribution,  if any, #
#    must include the following acknowlegement:                             #
#                                                                           #
#       "This product includes  software developed  by the Apache  Software #
#        Foundation <http://www.apache.org/>."                              #
#                                                                           #
#    Alternately, this acknowlegement may appear in the software itself, if #
#    and wherever such third-party acknowlegements normally appear.         #
#                                                                           #
# 4. The names "The Jakarta Project",  "Apache WebApp Module",  and "Apache #
#    Software Foundation"  must not be used to endorse or promote  products #
#    derived  from this  software  without  prior  written  permission. For #
#    written permission, please contact <apache@apache.org>.                #
#                                                                           #
# 5. Products derived from this software may not be called "Apache" nor may #
#    "Apache" appear in their names without prior written permission of the #
#    Apache Software Foundation.                                            #
#                                                                           #
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES #
# INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY #
# AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL #
# THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY #
# DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL #
# DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS #
# OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) #
# HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, #
# STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN #
# ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE #
# POSSIBILITY OF SUCH DAMAGE.                                               #
#                                                                           #
# ========================================================================= #
#                                                                           #
# This software  consists of voluntary  contributions made  by many indivi- #
# duals on behalf of the  Apache Software Foundation.  For more information #
# on the Apache Software Foundation, please see <http://www.apache.org/>.   #
#                                                                           #
# ========================================================================= #

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
autoconf

# ------------------------------------------------------------------------- #
# Finish up                                                                 # 
# ------------------------------------------------------------------------- #
echo ""
echo "--- All done"
