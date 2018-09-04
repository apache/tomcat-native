#!/bin/sh

# Small script to copy the libraries needed by tc-native.
TARGET=target/classes/`uname -s`-`uname -p`/lib
mkdir -p $TARGET

echo "The libraries with be copied to $TARGET"

TC_LIB=`find . -name *.so | head -1`

cp $TC_LIB $TARGET

NAMETC_LIB=`basename $TC_LIB`

for file in `ldd $TARGET/$NAMETC_LIB`
do
  lib=`echo $file | awk '$1 ~ /^\// '`
  echo $lib
  case $lib in
    *ssl*)
      cp $lib $TARGET
      ;;
    *crypto*)
      cp $lib $TARGET
      ;;
    *apr*)
      cp $lib $TARGET
      ;;
    esac
done
