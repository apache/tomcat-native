#
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
#

dnl
dnl TCN_FIND_APR: figure out where APR is located
dnl
AC_DEFUN(TCN_FIND_APR,[

  dnl use the find_apr.m4 script to locate APR. sets apr_found and apr_config
  APR_FIND_APR(,,1,[1])
  if test "$apr_found" = "no"
  then
    AC_MSG_ERROR(APR could not be located. Please use the --with-apr option.)
  fi

  sapr_pversion="`$apr_config --version`"
  if test -z "$sapr_pversion"
  then
    AC_MSG_ERROR(APR config could not be located. Please use the --with-apr option.)
  fi
  sapr_version="`echo $sapr_pversion|sed -e 's/\([a-z]*\)$/.\1/'`"
  tc_save_IFS=$IFS
  IFS=.
  set $sapr_version
  IFS=$tc_save_IFS
  decimal_apr_version=`printf %02d%02d%03d ${1} ${2} ${3}`
  if test "${decimal_apr_version}" -lt "0104003"
  then
    AC_MSG_ERROR(Found APR $sapr_version. You need version 1.4.3 or newer installed.)
  fi
  AC_MSG_NOTICE(APR $sapr_version detected.)

  APR_BUILD_DIR="`$apr_config --installbuilddir`"

  dnl make APR_BUILD_DIR an absolute directory (we'll need it in the
  dnl sub-projects in some cases)
  APR_BUILD_DIR="`cd $APR_BUILD_DIR && pwd`"

  APR_INCLUDES="`$apr_config --includes`"
  APR_LIBTOOL_LIBS="`$apr_config --link-libtool --libs`"
  APR_LIBS="`$apr_config --link-ld --libs`"
  APR_SO_EXT="`$apr_config --apr-so-ext`"
  APR_LIB_TARGET="`$apr_config --apr-lib-target`"

  AC_SUBST(APR_INCLUDES)
  AC_SUBST(APR_LIBTOOL_LIBS)
  AC_SUBST(APR_LIBS)
  AC_SUBST(APR_BUILD_DIR)
])

dnl --------------------------------------------------------------------------
dnl TCN_JDK
dnl
dnl Detection of JDK location and Java Platform (1.2, 1.3, 1.4, 1.5, 1.6)
dnl result goes in JAVA_HOME / JAVA_PLATFORM (2 -> 1.2 and higher)
dnl
dnl --------------------------------------------------------------------------
AC_DEFUN([TCN_FIND_JAVA],[
  AC_ARG_WITH(java-home,[  --with-java-home=DIR     Specify the location of your JDK installation],[
    AC_MSG_CHECKING([JAVA_HOME])
    if test -d "$withval"
    then
      JAVA_HOME="$withval"
      AC_MSG_RESULT([$JAVA_HOME])
    else
      AC_MSG_RESULT([failed])
      AC_MSG_ERROR([$withval is not a directory])
    fi
    AC_SUBST(JAVA_HOME)
  ])
  if test "x$JAVA_HOME" = x
  then
    AC_MSG_CHECKING([for JDK location])
    # Oh well, nobody set JAVA_HOME, have to guess
    # Check if we have java in the PATH.
    java_prog="`which java 2>/dev/null || true`"
    if test "x$java_prog" != x
    then
      java_bin="`dirname $java_prog`"
      java_top="`dirname $java_bin`"
      if test -f "$java_top/include/jni.h"
      then
        JAVA_HOME="$java_top"
        AC_MSG_RESULT([${java_top}])
      fi
    fi
  fi
  if test x"$JAVA_HOME" = x
  then
    AC_MSG_ERROR([Java Home not defined. Rerun with --with-java-home=[...] parameter])
  fi
])

AC_DEFUN([TCN_FIND_JDK_OS],[
  tempval=""
  JAVA_OS=""
  AC_ARG_WITH(os-type,[  --with-os-type[=SUBDIR]   Location of JDK os-type subdirectory.],
  [
    tempval=$withval
    if test ! -d "$JAVA_HOME/$tempval"
    then
      AC_MSG_ERROR(Not a directory: ${JAVA_HOME}/${tempval})
    fi
    JAVA_OS=$tempval
  ],
  [
    AC_MSG_CHECKING(for JDK os include directory)
    JAVA_OS=NONE
    if test -f $JAVA_HOME/$JAVA_INC/jni_md.h
    then
      JAVA_OS=""
    else
      for f in $JAVA_HOME/$JAVA_INC/*/jni_md.h
      do
        if test -f $f; then
            JAVA_OS=`dirname $f`
            JAVA_OS=`basename $JAVA_OS`
            echo " $JAVA_OS"
            break
        fi
      done
      if test "x$JAVA_OS" = "xNONE"; then
        AC_MSG_RESULT(Cannot find jni_md.h in ${JAVA_HOME}/${OS})
        AC_MSG_ERROR(You should retry --with-os-type=SUBDIR)
      fi
    fi
  ])
])

dnl TCN_HELP_STRING(LHS, RHS)
dnl Autoconf 2.50 can not handle substr correctly.  It does have
dnl AC_HELP_STRING, so let's try to call it if we can.
dnl Note: this define must be on one line so that it can be properly returned
dnl as the help string.
AC_DEFUN(TCN_HELP_STRING,[ifelse(regexp(AC_ACVERSION, 2\.1), -1, AC_HELP_STRING($1,$2),[  ]$1 substr([                       ],len($1))$2)])dnl

dnl
dnl TCN_CHECK_SSL_TOOLKIT
dnl
dnl Configure for the detected openssl toolkit installation, giving
dnl preference to "--with-ssl=<path>" if it was specified.
dnl
AC_DEFUN(TCN_CHECK_SSL_TOOLKIT,[
AC_MSG_CHECKING(for OpenSSL library)
AC_ARG_WITH(ssl,
[  --with-ssl[=PATH]   Build with OpenSSL [yes|no|path]],
    use_openssl="$withval", use_openssl="auto")

openssldirs="/usr /usr/local /usr/local/ssl /usr/pkg /usr/sfw"
if test "$use_openssl" = "auto"
then
    for d in $openssldirs
    do
        if test -f $d/include/openssl/opensslv.h
        then
            use_openssl=$d
            break
        fi
    done
fi
case "$use_openssl" in
    no)
        AC_MSG_RESULT(no)
        TCN_OPENSSL_INC=""
        USE_OPENSSL=""
        ;;
    auto)
        TCN_OPENSSL_INC=""
        USE_OPENSSL=""
        AC_MSG_RESULT(not found)
        ;;
    *)
        if test "$use_openssl" = "yes"
        then
            # User did not specify a path - guess it
            for d in $openssldirs
            do
                if test -f $d/include/openssl/opensslv.h
                then
                    use_openssl=$d
                    break
                fi
            done
            if test "$use_openssl" = "yes"
            then
                AC_MSG_RESULT(not found)
                AC_MSG_ERROR(
[OpenSSL was not found in any of $openssldirs; use --with-ssl=/path])
            fi
        fi
        USE_OPENSSL='-DOPENSSL'

        test -d $use_openssl/lib64 && ssllibdir=lib64 || ssllibdir=lib
        if test "$use_openssl" = "/usr"
        then
            TCN_OPENSSL_INC=""
            TCN_OPENSSL_LIBS="-lssl -lcrypto"
        else
            TCN_OPENSSL_INC="-I$use_openssl/include"
            case $host in
            *-solaris*)
                TCN_OPENSSL_LIBS="-L$use_openssl/$ssllibdir -R$use_openssl/$ssllibdir -lssl -lcrypto"
                ;;
            *-hp-hpux*)
                # By default cc/aCC on HP-UX IA64 will produce 32 bit output
                ssllibdir=lib/hpux32
                TCN_OPENSSL_LIBS="-L$use_openssl/$ssllibdir -lssl -lcrypto"
                ;;
            *linux*|*freebsd*)
                TCN_OPENSSL_LIBS="-L$use_openssl/$ssllibdir -Wl,-rpath,$use_openssl/$ssllibdir -lssl -lcrypto"
                ;;
            *)
                TCN_OPENSSL_LIBS="-L$use_openssl/$ssllibdir -lssl -lcrypto"
                ;;
            esac
        fi
        AC_MSG_RESULT(using openssl from $use_openssl/$ssllibdir and $use_openssl/include)

        saved_cflags="$CFLAGS"
        saved_libs="$LIBS"
        CFLAGS="$CFLAGS $TCN_OPENSSL_INC"
        LIBS="$LIBS $TCN_OPENSSL_LIBS"

AC_ARG_ENABLE(openssl-version-check,
[AC_HELP_STRING([--disable-openssl-version-check],
        [disable the OpenSSL version check])])
case "$enable_openssl_version_check" in
yes|'')
        AC_MSG_CHECKING(OpenSSL library version >= 1.0.2)
        AC_TRY_RUN([
#include <stdio.h>
#include <openssl/opensslv.h>
int main() {
        if (OPENSSL_VERSION_NUMBER >= 0x1000200fL)
            return (0);
    printf("\n\nFound   OPENSSL_VERSION_NUMBER %#010x (" OPENSSL_VERSION_TEXT ")\n",
        OPENSSL_VERSION_NUMBER);
    printf("Require OPENSSL_VERSION_NUMBER 0x1000200f or greater (1.0.2)\n\n");
        return (1);
}
        ],
        [AC_MSG_RESULT(ok)],
        [AC_MSG_ERROR(Your version of OpenSSL is not compatible with this version of tcnative)],
        [AC_MSG_RESULT(assuming target platform has compatible version)])
;;
no)
    AC_MSG_RESULT(Skipped OpenSSL version check)
;;
esac

        AC_MSG_CHECKING(for OpenSSL DSA support)
        if test -f $use_openssl/include/openssl/dsa.h
        then
            AC_DEFINE(HAVE_OPENSSL_DSA)
            AC_MSG_RESULT(yes)
        else
            AC_MSG_RESULT(no)
        fi
        CFLAGS="$saved_cflags"
        LIBS="$saved_libs"
        ;;
esac
if test "x$USE_OPENSSL" != "x"
then
    APR_ADDTO(TCNATIVE_PRIV_INCLUDES, [$TCN_OPENSSL_INC])
    APR_ADDTO(TCNATIVE_LDFLAGS, [$TCN_OPENSSL_LIBS])
    APR_ADDTO(CFLAGS, [-DHAVE_OPENSSL])
fi
])

dnl
dnl TCN_FIND_APR_FEATURE: figure out if APR feature is suipported
dnl
AC_DEFUN(TCN_FIND_APR_FEATURE,[
  saved_cflags="$CFLAGS"
  saved_libs="$LIBS"
  CFLAGS="$CFLAGS $APR_INCLUDES"
  LIBS="$LIBS $APR_LIBS"
  chk_result=0
  AC_CHECK_LIB(apr-1, $1,[chk_result=1])
  CFLAGS="$saved_cflags"
  LIBS="$saved_libs"
  if test "$chk_result" != "0"
  then
    APR_ADDTO(CFLAGS, [-DHAVE_$2])
  fi
])
