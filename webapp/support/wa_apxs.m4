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
  [WA_APXS],
  [
    wa_apxs_tempval="apxs"
    AC_MSG_CHECKING([for apxs name])
    AC_ARG_WITH(
      [apxs],
      [  --with-apxs[[=apxs]]      the Apache APXS utility to use],
      [
        case "${withval}" in
        ""|"yes"|"YES"|"true"|"TRUE")
          ;;
        "no"|"NO"|"false"|"FALSE")
          WA_ERROR([apxs required for compilation])
          ;;
        *)
          wa_apxs_tempval="${withval}"
          ;;
        esac
      ])
    AC_MSG_RESULT([${wa_apxs_tempval}])
    WA_PATH_PROG_FAIL([$1],[${wa_apxs_tempval}],[apxs])
    unset wa_apxs_tempval
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
    wa_apxs_check_tempval=""
    AC_MSG_CHECKING([for apxs version])
    if grep "STANDARD_MODULE_STUFF" "$2" > /dev/null ; then
      wa_apxs_check_tempval="1.3"
    elif grep "STANDARD20_MODULE_STUFF" "$2" > /dev/null ; then
      wa_apxs_check_tempval="2.0"
    else
      WA_ERROR([$2 invalid])
    fi
    AC_MSG_RESULT([$2 (${wa_apxs_check_tempval})])
    $1=${wa_apxs_check_tempval}
    unset wa_apxs_check_tempval
    
    AC_MSG_CHECKING([for apxs sanity])
    $2 -q CC > /dev/null 2>&1
    if test "$?" != "0" ; then
      WA_ERROR([apxs is unworkable])
    fi
    if test -z "`$2 -q CC`" ; then
      WA_ERROR([apxs cannot compile])
    fi
    AC_MSG_RESULT([ok])
  ])

dnl --------------------------------------------------------------------------
dnl WA_APXS_GET
dnl   Retrieve a value from APXS
dnl   $1 => Environment variable where the APXS value will be stored
dnl   $2 => Name of the APXS script (as returned by WA_APXS)
dnl   $3 => Name of the APXS value to retrieve
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_APXS_GET],
  [
    AC_MSG_CHECKING([for apxs $3 variable])
    wa_apxs_get_tempval=`"$2" -q "$3" 2> /dev/null`
    if test "$?" != "0" ; then
      WA_ERROR([cannot execute $2])
    fi
    AC_MSG_RESULT([${wa_apxs_get_tempval}])
    WA_APPEND([$1],[${wa_apxs_get_tempval}])
    unset wa_apxs_get_tempval
  ])

dnl --------------------------------------------------------------------------
dnl WA_APXS_SERVER
dnl   Retrieve the HTTP server version via APXS
dnl   $1 => Environment variable where the info string will be stored
dnl   $2 => Name of the APXS script (as returned by WA_APXS)
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_APXS_SERVER],
  [
    WA_APXS_GET([wa_apxs_server_sbindir],[$2],[SBINDIR])
    WA_APXS_GET([wa_apxs_server_target],[$2],[TARGET])
    wa_apxs_server_daemon="${wa_apxs_server_sbindir}/${wa_apxs_server_target}"
    WA_PATH_PROG_FAIL([wa_apxs_server_daemon],[${wa_apxs_server_daemon}],[httpd])
    AC_MSG_CHECKING([httpd version])
    wa_apxs_server_info="`${wa_apxs_server_daemon} -v | head -1`"
    wa_apxs_server_info="`echo ${wa_apxs_server_info} | cut -d: -f2`"
    wa_apxs_server_info="`echo ${wa_apxs_server_info}`"
    AC_MSG_RESULT([${wa_apxs_server_info}])
    $1="${wa_apxs_server_info}"
    unset wa_apxs_server_sbindir
    unset wa_apxs_server_target
    unset wa_apxs_server_daemon
    unset wa_apxs_server_info
  ])
