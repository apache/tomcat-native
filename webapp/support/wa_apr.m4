dnl  =========================================================================
dnl
dnl                  The Apache Software License,  Version 1.1
dnl
dnl           Copyright (c) 1999-2001 The Apache Software Foundation.
dnl                            All rights reserved.
dnl
dnl  =========================================================================
dnl
dnl  Redistribution and use in source and binary forms,  with or without modi-
dnl  fication, are permitted provided that the following conditions are met:
dnl
dnl  1. Redistributions of source code  must retain the above copyright notice
dnl     notice, this list of conditions and the following disclaimer.
dnl
dnl  2. Redistributions  in binary  form  must  reproduce the  above copyright
dnl     notice,  this list of conditions  and the following  disclaimer in the
dnl     documentation and/or other materials provided with the distribution.
dnl
dnl  3. The end-user documentation  included with the redistribution,  if any,
dnl     must include the following acknowlegement:
dnl
dnl        "This product includes  software developed  by the Apache  Software
dnl         Foundation <http://www.apache.org/>."
dnl
dnl     Alternately, this acknowlegement may appear in the software itself, if
dnl     and wherever such third-party acknowlegements normally appear.
dnl
dnl  4. The names "The Jakarta Project",  "Apache WebApp Module",  and "Apache
dnl     Software Foundation"  must not be used to endorse or promote  products
dnl     derived  from this  software  without  prior  written  permission. For
dnl     written permission, please contact <apache@apache.org>.
dnl
dnl  5. Products derived from this software may not be called "Apache" nor may
dnl     "Apache" appear in their names without prior written permission of the
dnl     Apache Software Foundation.
dnl
dnl  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES
dnl  INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY
dnl  AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL
dnl  THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY
dnl  DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL
dnl  DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS
dnl  OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION)
dnl  HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT,
dnl  STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
dnl  ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE
dnl  POSSIBILITY OF SUCH DAMAGE.
dnl
dnl  =========================================================================
dnl
dnl  This software  consists of voluntary  contributions made  by many indivi-
dnl  duals on behalf of the  Apache Software Foundation.  For more information
dnl  on the Apache Software Foundation, please see <http://www.apache.org/>.
dnl
dnl  =========================================================================

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

