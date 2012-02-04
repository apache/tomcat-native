#!/bin/sh

# Small script to get a recent openssl, with npn support
# Will run configure and build_libs to generate the .s files



SSL=openssl-1.0.1-beta2.tar.gz
APR=apr-1.4.5.tar.gz
mkdir -p deps

if [ ! -f deps/$SSL ] ; then
  curl http://openssl.org/source/$SSL -o deps/$SSL
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
  curl http://mirrors.ibiblio.org/apache/apr/$APR -o deps/$APR
fi 
if [ ! -d deps/src/apr ] ; then
  mkdir -p deps/src/apr 
  (cd deps/src/apr; tar  -xvz --strip-components=1 -f ../../$APR)
fi

  
  
 
