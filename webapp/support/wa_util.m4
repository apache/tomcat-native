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
dnl WA_PATH_DIR
dnl   Resolve the FULL path name of a directory.
dnl   $1 => The variable where the full path name will be stored.
dnl   $2 => The path to resolve.
dnl   $3 => The description of what we're trying to locate.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_PATH_DIR],
  [
    AC_MSG_CHECKING([for $3 path])
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
