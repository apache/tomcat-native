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
dnl Author Andy Armstrong <andy@tagish.com>
dnl Shamelessly cribbed from  Henri Gomez <hgomez@apache.org>
dnl
dnl He was inspired by Pier works on webapp m4 macros :)
dnl 
dnl Version $Id$
dnl --------------------------------------------------------------------------

dnl --------------------------------------------------------------------------
dnl JK_DOMHOME
dnl   Set the Domino Home directory.
dnl   $1 => Domino Name
dnl   $2 => Domino VarName
dnl   $3 => File which should be present
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_DOMHOME],
  [
    tempval=""

    AC_MSG_CHECKING([for $1 location])
    AC_ARG_WITH(
      [$1],
      [  --with-$1=DIR      Location of $1 ],
      [ 
        case "${withval}" in
        ""|"yes"|"YES"|"true"|"TRUE")
          ;;
        "no"|"NO"|"false"|"FALSE")
          AC_MSG_ERROR(valid $1 location required)
          ;;
        *)
          tempval="${withval}"

          if ${TEST} ! -d ${tempval} ; then
            AC_MSG_ERROR(Not a directory: ${tempval})
          fi

          if ${TEST} ! -f ${tempval}/$3; then
            AC_MSG_ERROR(can't locate ${tempval}/$3)
          fi
          ;;
        esac
      ])  

      if ${TEST} -z "$tempval" ; then
        AC_MSG_RESULT(not provided)
      else
        [$2]=${tempval}
        AC_MSG_RESULT(${[$2]})
      fi

      unset tempval
  ])

dnl vi:set sts=2 sw=2 autoindent:
