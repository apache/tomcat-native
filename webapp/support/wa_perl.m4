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
  [WA_PERL],
  [
    wa_perl_enabled=""
    wa_perl_tempval=""
    AC_MSG_CHECKING([if perl is enabled])
    AC_ARG_WITH(
      [perl],
      [  --with-perl[[=perl]]      the Perl interpreter to use],
      [
        case "${withval}" in
        ""|"yes"|"YES"|"true"|"TRUE")
          AC_MSG_RESULT([yes])
          wa_perl_enabled="yes"
          wa_perl_tempval=""
          ;;
        "no"|"NO"|"false"|"FALSE")
          AC_MSG_RESULT([no])
          wa_perl_enabled="no"
          wa_perl_tempval=""
          ;;
        *)
          AC_MSG_RESULT([yes (${withval})])
          wa_perl_enabled="yes"
          WA_PATH_PROG([wa_perl_tempval],[${withval}],[perl])
          if test -z "${wa_perl_tempval}" ; then
            AC_MSG_ERROR([${withval} is invalid])
          fi
          ;;
        esac
      ],[
        AC_MSG_RESULT([guessing])
      ])

    if test "${wa_perl_enabled}" = "no" ; then
      wa_perl_tempval=""
    else
      if test -z "${wa_perl_tempval}" ; then
        WA_PATH_PROG([wa_perl_tempval],[perl],[perl])
      fi
      if test -z "${wa_perl_tempval}" ; then
        if test "${wa_perl_enabled}" = "yes" ; then
          AC_MSG_ERROR([perl interpreter cannot be found])
          exit 1
        fi
      fi
    fi

    if test -n "${wa_perl_tempval}" ; then
      AC_MSG_CHECKING([if ${wa_perl_tempval} is working])

      wa_perl_enabled=`echo 'printf "%vd\n", $^V;' | ${wa_perl_tempval}`
      if test -z "${wa_perl_enabled}" ; then
        WA_ERROR([perl misconfigured, reconfigure with --without-perl])
      else
        AC_MSG_RESULT([${wa_perl_enabled}])
      fi
    fi
    
    $1="${wa_perl_tempval}"
    unset wa_perl_tempval
    unset wa_perl_enabled
  ])
