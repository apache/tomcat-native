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

#
# BEFORE releasing don't forget to edit and commit
#        native/include/tcn_version.h
#        native/os/win32/libtcnative.rc

# Default place to look for apr source.  Can be overridden with
#   --with-apr=[directory]
apr_src_dir=`pwd`/native/srclib/apr

GITBASE=https://gitbox.apache.org/repos/asf/tomcat-native.git
TCJAVA_GITBASE=https://gitbox.apache.org/repos/asf/tomcat.git

# Set the environment variable that stops OSX storing extended
# attributes in tar archives etc. with a file starting with ._
COPYFILE_DISABLE=1
export COPYFILE_DISABLE

JKJNIEXT=""
JKJNIHASH=""
JKJNIVER=""
JKJNIREL=""
JKJNIFORCE=""

for o
do
    case "$o" in
    -*=*) a=`echo "$o" | sed 's/^[-_a-zA-Z0-9]*=//'` ;;
       *) a='' ;;
    esac
    case "$o" in
        -f )
            JKJNIFORCE=1
            shift
            ;;
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
    echo "Unknown Git tag/branch"
    echo "Use:"
    echo "  --ver=<tag>|1.1.x|main|."
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
        case ${i} in
          w3m)
            EXPOPTS="${w3m_opts}"
            ;;
          elinks)
            EXPOPTS="${elinks_opts}"
            ;;
          links)
            EXPOPTS="${links_opts}"
            ;;
        esac
        echo "Using: ${EXPTOOL} ${EXPOPTS} ..."
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
PERL="`which perl 2>/dev/null || type perl 2>&1`"
if [ -x "$PERL" ]; then
    echo "Using $PERL"
else
    echo ""
    echo "Cannot find perl"
    echo "Make sure you have perl in the PATH"
    echo ""
    exit 1
fi

if [ "x$JKJNIEXT" = "xmain" ]; then
    JKJNIHASH=`git ls-remote $GITBASE refs/heads/main | awk '{print $1}'`
    JKJNIVER="$JKJNIEXT-$JKJNIHASH"
elif [ "x$JKJNIEXT" = "x1.1.x" ]; then
    JKJNIHASH=`git ls-remote $GITBASE refs/heads/1.1.x | awk '{print $1}'`
    JKJNIVER="$JKJNIEXT-$JKJNIHASH"
elif [ "x$JKJNIEXT" = "x." ]; then
    JKJNIHASH=`git rev-parse --short HEAD`
    JKJNIVER="HEAD-$JKJNIHASH"
else
    JKJNIHASH=`git ls-remote $GITBASE refs/tags/$JKJNIEXT | awk '{print $1}'`
    JKJNIVER="$JKJNIEXT"
    JKJNIREL=1
fi
echo "Using GIT repo       : \`${GITBASE}\`"
echo "Using version        : \`${JKJNIVER}\`"

if [ ! -d .git ]; then
    rm -rf tmp-clone
    git clone --no-checkout $GITBASE tmp-clone
    mv tmp-clone/.git .
    rm -rf tmp-clone
    # Checking for recentness of git subtree
    git checkout --force ${JKJNIHASH}
else
    # Checking for recentness of git subtree
    git checkout ${JKJNIHASH}
fi

if [ ! -d .git/refs/remotes/10.0.x ]; then
    git remote add -f 10.0.x ${TCJAVA_GITBASE}
fi
git remote update 10.0.x
diffcount=`git diff HEAD remotes/10.0.x/main java/org/apache/tomcat/jni | wc -l`

if [ $diffcount -ne 0 ]; then
    echo "WARNING: git subtree is not up to date with"
    echo "         $TCJAVA_GITBASE"
    echo "         Either correct now by running"
    echo "         'git rm -rf java/org/apache/tomcat/jni'"
    echo "         'git read-tree --prefix=java/org/apache/tomcat/jni/ -u 10.0.x/main:java/org/apache/tomcat/jni'"
    echo "         'git commit'"
    echo "         or run this script with -f (force)"
    if [ "X$JKJNIFORCE" = "X1" ]
    then
        sleep 3
        echo "FORCED run chosen"
    else
        exit 1
    fi
fi

JKJNIDIST=tomcat-native-${JKJNIVER}-src

rm -rf ${JKJNIDIST}
mkdir -p ${JKJNIDIST}
git archive ${JKJNIHASH} | tar -x -C ${JKJNIDIST}
if [ $? -ne 0 ]; then
    echo ""
    echo "git export failed"
    echo ""
    exit 1
fi
rm -f ${JKJNIDIST}/KEYS ${JKJNIDIST}/download_deps.sh

# check the release if release.
if [ "x$JKJNIREL" = "x1" ]; then
    grep TCN_IS_DEV_VERSION ${JKJNIDIST}/native/include/tcn_version.h | grep 0
    if [ $? -ne 0 ]; then
        echo "Check: ${JKJNIDIST}/native/include/tcn_version.h says -dev"
        echo "Check TCN_IS_DEV_VERSION - Aborting"
        exit 1
    fi
    WIN_VERSION=`grep TCN_VERSION ${JKJNIDIST}/native/os/win32/libtcnative.rc | grep define | awk ' { print $3 } '`
    if [ "x\"$JKJNIVER\"" != "x$WIN_VERSION" ]; then
        echo "Check: ${JKJNIDIST}/native/os/win32/libtcnative.rc says $WIN_VERSION (FILEVERSION, PRODUCTVERSION, TCN_VERSION)"
        echo "Must be $JKJNIVER - Aborting"
        exit 1
    fi
else
    echo "Not a release"
fi

top="`pwd`"
cd ${JKJNIDIST}/xdocs

# Make docs
ant
if [ $? -ne 0 ]; then
    echo ""
    echo "ant (building docs failed)"
    echo ""
    exit 1
fi

$EXPTOOL $EXPOPTS ../build/docs/miscellaneous/changelog.html > ../CHANGELOG.txt 2>/dev/null
if [ $? -ne 0 ]; then
    echo ""
    echo "$EXPTOOL $EXPOPTS ../build/docs/miscellaneous/changelog.html failed"
    echo ""
    exit 1
fi
# Remove page navigation data from converted file.
cp -p ../CHANGELOG.txt ../CHANGELOG.txt.tmp
awk '/Preface/ {o=1} o>0' ../CHANGELOG.txt.tmp > ../CHANGELOG.txt
rm ../CHANGELOG.txt.tmp

cd "$top"
mv ${JKJNIDIST}/build/docs ${JKJNIDIST}/docs
rm -rf ${JKJNIDIST}/build

# Prebuild (create configure)
cd ${JKJNIDIST}/native
./buildconf --with-apr=$apr_src_dir || exit 1

cd "$top"
# Create source distribution
tar -cf - ${JKJNIDIST} | gzip -c9 > ${JKJNIDIST}.tar.gz || exit 1

# Create Win32 source distribution
JKWINDIST=tomcat-native-${JKJNIVER}-win32-src
rm -rf ${JKWINDIST}
mkdir -p ${JKWINDIST}
git -c core.autocrlf=true -c core.eol=crlf archive ${JKJNIHASH} | tar -x -C ${JKWINDIST}
if [ $? -ne 0 ]; then
    echo ""
    echo "git export failed"
    echo ""
    exit 1
fi
rm -f ${JKWINDIST}/KEYS ${JKWINDIST}/download_deps.sh

top="`pwd`"
cd ${JKWINDIST}/xdocs

# Make docs
ant
if [ $? -ne 0 ]; then
    echo ""
    echo "ant (building docs failed)"
    echo ""
    exit 1
fi

cd "$top"
cp ${JKJNIDIST}/CHANGELOG.txt ${JKWINDIST}

mv ${JKWINDIST}/build/docs ${JKWINDIST}/docs
rm -rf ${JKWINDIST}/build
for i in LICENSE NOTICE README.txt TODO.txt
do
    $PERL ${JKWINDIST}/native/build/lineends.pl --cr ${JKWINDIST}/${i}
done
$PERL ${JKWINDIST}/native/build/lineends.pl --cr ${JKWINDIST}/CHANGELOG.txt

zip -9rqyo ${JKWINDIST}.zip ${JKWINDIST}
