dnl
dnl Copyright 1999-2004 The Apache Software Foundation
dnl
dnl Licensed under the Apache License, Version 2.0 (the "License");
dnl you may not use this file except in compliance with the License.
dnl You may obtain a copy of the License at
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is distributed on an "AS IS" BASIS,
dnl WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl See the License for the specific language governing permissions and
dnl limitations under the License.
dnl

dnl --------------------------------------------------------------------------
dnl Author Pier Fumagalli <pier@betaversion.org>
dnl Version $Id$
dnl --------------------------------------------------------------------------

dnl --------------------------------------------------------------------------
dnl WA_APR
dnl   Locate the Apache APR source directory.
dnl   $1 => Environment variable name where the APR directory will be stored
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_APR],
  [
    if test -z "${srcdir}" ; then
      wa_apr_tempval="apr"
    else
      wa_apr_tempval="${srcdir}/apr"
    fi
    AC_MSG_CHECKING([for apr sources])
    AC_ARG_WITH(
      [apr],
      [  --with-apr[[=apr]]        the Apache Portable Runtime library to use],
      [
        case "${withval}" in
        ""|"yes"|"YES"|"true"|"TRUE")
          ;;
        "no"|"NO"|"false"|"FALSE")
          WA_ERROR([apr library sources required for compilation])
          ;;
        *)
          wa_apr_tempval="${withval}"
          ;;
        esac
      ])
    AC_MSG_RESULT([${wa_apr_tempval}])
    WA_PATH_DIR($1,[${wa_apr_tempval}],[apr sources])
    
    unset wa_apr_tempval
  ])

dnl --------------------------------------------------------------------------
dnl WA_APR_GET
dnl   Retrieve a value from the configured APR source tree
dnl   $1 => Environment variable name for the returned value
dnl   $2 => APR sources directory as returned by WA_APR
dnl   $3 => APR variable name (found in $2/apr-config)
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_APR_GET],
  [
    AC_MSG_CHECKING([for apr $3 variable])
    if test ! -f "$2/apr-config" ; then
      WA_ERROR([cannot find apr-config file in $2])
    fi
    wa_apr_get_tempval=`cat $2/apr-config | grep "^$3=" 2> /dev/null`
    if test -z "${wa_apr_get_tempval}" ; then
      WA_ERROR([value for $3 not specified in $2/apr-config])
    fi
    wa_apr_get_tempval=`echo ${wa_apr_get_tempval} | sed 's/^$3="//g'`
    wa_apr_get_tempval=`echo ${wa_apr_get_tempval} | sed 's/"$//g'`
    WA_APPEND([$1],[${wa_apr_get_tempval}])
    AC_MSG_RESULT([${wa_apr_get_tempval}])
    unset wa_apr_get_tempval
  ])

dnl --------------------------------------------------------------------------
dnl WA_APR_LIB
dnl   Retrieve the name of the library for -l$(APR_LIB)
dnl   $1 => Environment variable name for the returned value
dnl   $2 => APR sources directory as returned by WA_APR
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_APR_LIB],
  [
    AC_MSG_CHECKING([for apr APR_LIB])
    if test ! -f "$2/apr-config" ; then
      WA_ERROR([cannot find apr-config file in $2])
    fi
    wa_apr_get_tempval=`$2/apr-config --link-libtool 2> /dev/null`
    if test -z "${wa_apr_get_tempval}" ; then
      WA_ERROR([$2/apr-config --link-libtool failed])
    fi
    wa_apr_get_tempval=`basename ${wa_apr_get_tempval} |  sed 's/lib//g'`
    wa_apr_get_tempval=`echo ${wa_apr_get_tempval} | sed 's/\.la//g'`
    WA_APPEND([$1],[${wa_apr_get_tempval}])
    AC_MSG_RESULT([${wa_apr_get_tempval}])
    unset wa_apr_get_tempval
  ])

dnl --------------------------------------------------------------------------
dnl WA_APR_LIBNAME
dnl   Retrieve the complete name of the library.
dnl   $1 => Environment variable name for the returned value
dnl   $2 => APR sources directory as returned by WA_APR
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_APR_LIBNAME],
  [
    AC_MSG_CHECKING([for apr APR_LIBNAME])
    if test ! -f "$2/apr-config" ; then
      WA_ERROR([cannot find apr-config file in $2])
    fi
    wa_apr_get_tempval=`$2/apr-config --link-libtool 2> /dev/null`
    if test -z "${wa_apr_get_tempval}" ; then
      WA_ERROR([$2/apr-config --link-libtool failed])
    fi
    wa_apr_get_tempval=`basename ${wa_apr_get_tempval}`
    WA_APPEND([$1],[${wa_apr_get_tempval}])
    AC_MSG_RESULT([${wa_apr_get_tempval}])
    unset wa_apr_get_tempval
  ])

