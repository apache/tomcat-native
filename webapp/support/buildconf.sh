#!/bin/sh

# Author Jon S. Stevens <mailto:jon@latchkey.com>
# Author Pier Fumagalli <pier.fumagalli@sun.com>
#
# This script is used to build the "configure" script from
# the "configure.in". If you check these sources out of CVS,
# you will need to execute this script first.

if [ ! -f ./configure.in ]
then
    echo "Cannot find \"configure.in\" file."
    exit 1
fi
if [ ! -d ./apr ]
then
    mkdir apr
    echo "Don't a forget to put a copy of the APR sources in `pwd`/apr/"
fi
autoconf
