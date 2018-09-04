#!/bin/sh

# Small script to copy the libraries needed by tc-native.
TC_OS=`uname -s`
LIB_SUFIX=so
LDD_TOOL=ldd
case $TC_OS in
  Darwin)
    echo "Using Darwin!!!"
    LIB_SUFIX=dylib
    LDD_TOOL="otool -L"
    ;;
  CYGWIN*)
    echo "Using CYGWIN!!!"
    LIB_SUFIX=dll
    ;;
esac

TARGET=target/classes/`uname -s`-`uname -p`/lib
rm -rf $TARGET
mkdir -p $TARGET

echo "The libraries will be copied to $TARGET"

TC_LIB=`find . -name *.$LIB_SUFIX | head -1`

cp $TC_LIB $TARGET

NAMETC_LIB=`basename $TC_LIB`

for file in `$LDD_TOOL $TARGET/$NAMETC_LIB`
do
  lib=`echo $file | awk '$1 ~ /^\// '`
  case $lib in
    *ssl*)
      cp $lib $TARGET
      ;;
    *crypto*)
      cp $lib $TARGET
      ;;
    *libapr*)
      cp $lib $TARGET
      ;;
    esac
done
