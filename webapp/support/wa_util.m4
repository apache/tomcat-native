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
dnl WA_ERROR
dnl   Dump an error message and make sure we actually exit.
dnl   $1 => Message to dump for error.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_ERROR],
  [
    AC_MSG_RESULT([error])
    AC_MSG_ERROR([$1])
    exit 1
  ])

dnl --------------------------------------------------------------------------
dnl WA_HEADER
dnl   Dump an extra header to the standard output
dnl   $1 => Message of the header to dump.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_HEADER],
  [
    AC_MSG_RESULT([])
    AC_MSG_RESULT([$1])
  ])

dnl --------------------------------------------------------------------------
dnl WA_VARIABLE
dnl   Initialize a substituted (global) variable with a zero-length string.
dnl   $1 => The environment variable name.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_VARIABLE],
  [
    $1=""
    AC_SUBST([$1])
  ])

dnl --------------------------------------------------------------------------
dnl WA_APPEND
dnl   Append the extra value to the variable specified if and only if the
dnl   value is not already in there.
dnl   $1 => The environment variable name.
dnl   $2 => The extra value
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_APPEND],
  [
    wa_append_tempval="`echo $2`"
    if test -n "${wa_append_tempval}" ; then
      if test -z "${$1}" ; then
        $1="${wa_append_tempval}"
      else 
        wa_append_found=""
        for wa_append_current in ${$1} ; do
          if test "${wa_append_current}" = "${wa_append_tempval}" ; then
            wa_append_found="yes"
          fi
        done
        if test -z "${wa_append_found}" ; then
          $1="${$1} ${wa_append_tempval}"
        fi
        unset wa_append_found
        unset wa_append_current
      fi
    fi 
    unset wa_append_tempval
  ])

dnl --------------------------------------------------------------------------
dnl WA_PATH_PROG
dnl   Resolve the FULL path name of an executable.
dnl   $1 => The variable where the full path name will be stored.
dnl   $2 => The executable to resolve.
dnl   $3 => The description of what we're trying to locate.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_PATH_PROG],
  [
    wa_path_prog_tempval="`echo $2`"
    if test -x "${wa_path_prog_tempval}" ; then
      wa_path_prog_tempdir=`dirname "${wa_path_prog_tempval}"`
      wa_path_prog_tempfil=`basename "${wa_path_prog_tempval}"`
      WA_PATH_DIR([wa_path_prog_tempdir],[${wa_path_prog_tempdir}],[$3])
      AC_MSG_CHECKING([for $3])
      $1="${wa_path_prog_tempdir}/${wa_path_prog_tempfil}"
      AC_MSG_RESULT([${$1}])
    else
      AC_PATH_PROG($1,[${wa_path_prog_tempval}])
    fi
  ])


dnl --------------------------------------------------------------------------
dnl WA_PATH_PROG_FAIL
dnl   Resolve the FULL path name of an executable and fail if not found.
dnl   $1 => The variable where the full path name will be stored.
dnl   $2 => The executable to resolve.
dnl   $3 => The description of what we're trying to locate.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_PATH_PROG_FAIL],
  [
    WA_PATH_PROG([$1],[$2],[$3])
    AC_MSG_CHECKING([for $3 availability])
    if test -z "${$1}" ; then
      AC_MSG_ERROR([cannot find $3 "${wa_apxs_tempval}"])
      exit 1
    fi
    AC_MSG_RESULT([${$1}])
  ])


dnl --------------------------------------------------------------------------
dnl WA_PATH_DIR
dnl   Resolve the FULL path name of a directory.
dnl   $1 => The variable where the full path name will be stored.
dnl   $2 => The path to resolve.
dnl   $3 => The description of what we're trying to locate.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_PATH_DIR],
  [
    AC_MSG_CHECKING([for $3 directory path])
    wa_path_dir_tempval="`echo $2`"
    if test -d "${wa_path_dir_tempval}" ; then
      wa_path_dir_curdir="`pwd`"
      cd "${wa_path_dir_tempval}"
      wa_path_dir_newdir="`pwd`"
      $1="${wa_path_dir_newdir}"
      AC_MSG_RESULT([${wa_path_dir_newdir}])
      cd "${wa_path_dir_curdir}"
      unset wa_path_dir_curdir
      unset wa_path_dir_newdir
    else
      WA_ERROR([directory ${wa_path_dir_tempval} not found])
    fi
    unset wa_path_dir_tempval
  ])

dnl --------------------------------------------------------------------------
dnl WA_HELP
dnl   Do some autoconf magic.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_HELP],
  [
    ECHO_N="${ECHO_N} + "
    m4_divert_once(
      [PARSE_ARGS],
      [
        if test "$ac_init_help" = "long" ; then
          ac_init_help="modified"
          echo "Configuration of AC_PACKAGE_STRING:"
          echo ""
          echo "Usage: [$]0 [[OPTION]]"
        fi
      ])
  ])
