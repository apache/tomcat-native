#!/bin/sh

# @author Jon S. Stevens <jon@latchkey.com>
#
# This script is used to build the "configure" script from
# the "configure.in". If you check these sources out of CVS,
# you will need to execute this script first.

if [ ! -d ./apr ]
then
    mkdir apr
    echo "Don't a forget to put a copy of the APR sources in `pwd`/apr/"
fi
autoconf
