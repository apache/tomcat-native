#! /bin/sh

# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


echo "buildconf: checking installation..."

# any python
python=${PYTHON-`build/PrintPath python3 python2 python`}
if test -z "$python"; then
echo "buildconf: python not found."
echo "           You need python installed"
echo "           to build Tomcat Native from SVN."
exit 1
else
py_version=`$python -c 'import sys; print sys.version' 2>&1|sed 's/ .*//;q'`
echo "buildconf: python version $py_version (ok)"
fi

# autoconf 2.68 or newer
ac_version=`${AUTOCONF:-autoconf} --version 2>/dev/null|sed -e 's/^[^0-9]*//;s/[a-z]* *$//;q'`
if test -z "$ac_version"; then
echo "buildconf: autoconf not found."
echo "           You need autoconf version 2.68 or newer installed"
echo "           to build Tomcat Native from SVN."
exit 1
fi
IFS=.; set $ac_version; IFS=' '
if test "$1" = "2" -a "$2" -lt "68" || test "$1" -lt "2"; then
echo "buildconf: autoconf version $ac_version found."
echo "           You need autoconf version 2.68 or newer installed"
echo "           to build Tomcat Native from SVN."
exit 1
else
echo "buildconf: autoconf version $ac_version (ok)"
fi

# Sample libtool --version outputs:
# ltmain.sh (GNU libtool) 1.3.3 (1.385.2.181 1999/07/02 15:49:11)
# ltmain.sh (GNU libtool 1.1361 2004/01/02 23:10:52) 1.5a
# output is multiline from 1.5 onwards

# Require libtool 1.4 or newer
libtool=`build/PrintPath glibtool libtool libtool15 libtool14`
lt_pversion=`$libtool --version 2>/dev/null|sed -e 's/([^)]*)//g;s/^[^0-9]*//;s/[- ].*//g;q'`
if test -z "$lt_pversion"; then
echo "buildconf: libtool not found."
echo "           You need libtool version 1.4 or newer installed"
echo "           to build Tomcat Native from SVN."
exit 1
fi
lt_version=`echo $lt_pversion|sed -e 's/\([a-z]*\)$/.\1/'`
IFS=.; set $lt_version; IFS=' '
lt_status="good"
if test "$1" = "1"; then
   if test "$2" -lt "4"; then
      lt_status="bad"
   fi
fi
if test $lt_status = "good"; then
   echo "buildconf: libtool version $lt_pversion (ok)"
   exit 0
fi

echo "buildconf: libtool version $lt_pversion found."
echo "           You need libtool version 1.4 or newer installed"
echo "           to build Tomcat Native from SVN."

exit 1
