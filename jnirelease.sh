#!/bin/sh
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Default place to look for apr source.  Can be overridden with 
#   --with-apr=[directory]
apr_src_dir=`pwd`/srclib/apr
JKJNIEXT=""
JKJNIVER=""
SVNBASE=https://svn.apache.org/repos/asf/tomcat/native

for o
do
    case "$o" in
    -*=*) a=`echo "$o" | sed 's/^[-_a-zA-Z0-9]*=//'` ;;
       *) a='' ;;
    esac
    case "$o" in
        --ver*=*    )
            JKJNIEXT="$a"
            shift
            ;;
        --with-apr=* )
            apr_src_dir="$a" ;
            shift ;;
        *  )
        echo ""
        echo "Usage: jnirelease.sh [options]"
        echo "  --ver[sion]=<version>  Tomcat Native version"
        echo "  --with-apr=<directory> APR sources directory"
    echo ""
        exit 1
        ;;
    esac
done


if [ -d "$apr_src_dir" ]; then
    echo ""
    echo "Using apr source from: \`$apr_src_dir'"
else
    echo ""
    echo "Problem finding apr source in: \`$apr_src_dir'"
    echo "Use:"
    echo "  --with-apr=<directory>"
    echo ""
    exit 1
fi

if [ "x$JKJNIEXT" = "x" ]; then
    echo ""
    echo "Unknown SVN version"
    echo "Use:"
    echo "  --ver=<version>|<branch>|trunk"
    echo ""
    exit 1
fi

# Check for links, elinks or w3m
w3m_opts="-dump -cols 80 -t 4 -S -O iso-8859-1 -T text/html"
elinks_opts="-dump -dump-width 80 -dump-charset iso-8859-1 -no-numbering -no-references -no-home"
links_opts="-dump -width 80 -codepage iso-8859-1 -no-g -html-numbered-links 0"
EXPTOOL=""
EXPOPTS=""
for i in w3m elinks links
do
    EXPTOOL="`which $i 2>/dev/null || type $i 2>&1`"
    if [ -x "$EXPTOOL" ]; then
        EXPOPTS="`eval echo \"\$$i_opts\"`"
        break
    fi
done
if [ ! -x "$EXPTOOL" ]; then
    echo ""
    echo "Cannot find html export tool"
    echo "Make sure you have either w3m elinks or links in the PATH"
    echo ""
    exit 1
fi

echo $JKJNIEXT | egrep -e 'x$' > /dev/null 2>&1
if [ $? -eq 0 ]; then
    USE_BRANCH=1
else
    USE_BRANCH=0
fi

JKJNISVN=$SVNBASE/${JKJNIEXT}
if [ "x$JKJNIEXT" = "xtrunk" ]; then
    JKJNIVER=`svn info ${JKJNISVN} | awk '$1 == "Revision:" {print $2}'`
elif [ $USE_BRANCH -eq 1 ]; then
    JKJNIBRANCH=${JKJNIEXT}
    JKJNISVN=$SVNBASE/branches/${JKJNIBRANCH}
    JKJNIVER=${JKJNIBRANCH}-`svn info ${JKJNISVN} | awk '$1 == "Revision:" {print $2}'`
else
    JKJNIVER=$JKJNIEXT
    JKJNISVN="${SVNBASE}/tags/TOMCAT_NATIVE_`echo $JKJNIVER | sed 's/\./_/g'`"
fi
echo "Using SVN repo       : \`${JKJNISVN}'"
echo "Using version        : \`${JKJNIVER}'"


JKJNIDIST=tomcat-native-${JKJNIVER}-src

rm -rf ${JKJNIDIST}

svn export ${JKJNISVN} ${JKJNIDIST}
if [ $? -ne 0 ]; then
    echo "svn export failed"
    exit 1
fi

top="`pwd`"
cd ${JKJNIDIST}/xdocs
ant
$EXPTOOL $EXPOPTS ../build/docs/miscellaneous/printer/changelog.html > ../CHANGELOG.txt 2>/dev/null
cd "$top"
mv ${JKJNIDIST}/build/docs ${JKJNIDIST}/docs
rm -rf ${JKJNIDIST}/build

#
# Prebuild
cd ${JKJNIDIST}/native
./buildconf --with-apr=$apr_src_dir
cd "$top"
# Create source distribution
tar -cf - ${JKJNIDIST} | gzip -c9 > ${JKJNIDIST}.tar.gz
#
# Create Win32 source distribution
JKWINDIST=tomcat-native-${JKJNIVER}-win32-src
rm -rf ${JKWINDIST}
mkdir -p ${JKWINDIST}
svn export --native-eol CRLF ${JKJNISVN}/native ${JKWINDIST}/native
svn cat ${JKJNISVN}/LICENSE > ${JKWINDIST}/LICENSE
svn cat ${JKJNISVN}/NOTICE > ${JKWINDIST}/NOTICE
svn cat ${JKJNISVN}/README.txt > ${JKWINDIST}/README.txt
cp ${JKJNIDIST}/CHANGELOG.txt ${JKWINDIST}/
zip -9rqyo ${JKWINDIST}.zip ${JKWINDIST}
