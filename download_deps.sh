#!/bin/sh

# Small script to get a recent openssl, with npn support
# Will run configure and build_libs to generate the .s files

SSL=openssl-1.1.1n.tar.gz
APR=apr-1.7.0.tar.gz
mkdir -p deps

if [ ! -f deps/$SSL ] ; then
  curl https://www.openssl.org/source/$SSL -o deps/$SSL
fi 
if [ ! -d deps/src/openssl ] ; then
  mkdir -p deps/src/openssl 
  (cd deps/src/openssl; tar  -xvz --strip-components=1 -f ../../$SSL)
fi
if [ ! -f deps/src/openssl/Makefile ] ; then
  (cd deps/src/openssl; ./config )
fi 

(cd deps/src/openssl; make build_libs )

if [ ! -f deps/$APR ] ; then
  curl https://archive.apache.org/dist/apr/$APR -o deps/$APR
fi 
if [ ! -d deps/src/apr ] ; then
  mkdir -p deps/src/apr 
  (cd deps/src/apr; tar  -xvz --strip-components=1 -f ../../$APR)
fi
