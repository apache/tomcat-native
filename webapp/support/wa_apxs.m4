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
dnl WA_APXS
dnl   Locate the APXS script
dnl   $1 => Environment variable where the APXS script name will be stored
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_APXS],
  [
    tempval="apxs"
    AC_MSG_CHECKING([for apxs name])
    AC_ARG_WITH(
      [apxs],
      [  --with-apxs[[=apxs]]      the Apache "apxs" utility to use.],
      [
        case "${withval}" in
        ""|"yes"|"YES"|"true"|"TRUE")
          ;;
        "no"|"NO"|"false"|"FALSE")
          WA_ERROR([apxs required for compilation])
          ;;
        *)
          tempval="${withval}"
          ;;
        esac
      ])
    AC_MSG_RESULT([${tempval}])
    if test -x "${tempval}" ; then
      tempdir=`dirname "${tempval}"`
      tempfil=`basename "${tempval}"`
      WA_PATH_DIR([tempdir],[${tempdir}],[apxs])
      $1="${tempdir}/${tempfil}"
    else
      AC_PATH_PROG($1,[${tempval}])
    fi
    
    if test -z "${$1}" ; then
      AC_MSG_ERROR([cannot find apxs utility "${tempval}"])
      exit 1
    fi
    unset tempval
    AC_SUBST($1)
  ])

dnl --------------------------------------------------------------------------
dnl WA_APXS_CHECK
dnl   Check that APXS is actually workable, and return its version number
dnl   $1 => Environment variable where the APXS version will be stored
dnl   $2 => Name of the APXS script (as returned by WA_APXS)
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_APXS_CHECK],
  [
    tempval=""
    AC_MSG_CHECKING([for apxs version])
    if grep "STANDARD_MODULE_STUFF" "$2" > /dev/null ; then
      tempval="1.3"
    elif grep "STANDARD20_MODULE_STUFF" "$2" > /dev/null ; then
      tempval="2.0"
    else
      WA_ERROR([$2 invalid])
    fi
    AC_MSG_RESULT([$2 (${tempval})])
    $1=${tempval}
    unset tempval
    
    AC_MSG_CHECKING([for apxs sanity])
    $2 -q CC > /dev/null 2>&1
    if test "$?" != "0" ; then
      WA_ERROR([apxs is unworkable])
    fi
    if test -z "`$2 -q CC`" ; then
      WA_ERROR([apxs cannot compile])
    fi
    AC_MSG_RESULT([ok])

    AC_SUBST($1)
  ])
