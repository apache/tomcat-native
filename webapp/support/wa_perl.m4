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
