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
dnl Author Henri Gomez <hgomez@slib.fr>
dnl
dnl Inspired by Pier works on webapp m4 macros :)
dnl 
dnl Version $Id$
dnl --------------------------------------------------------------------------

dnl --------------------------------------------------------------------------
dnl JK_WS_DIR
dnl   Set the WebServer source dir.
dnl   $1 => Webserver name
dnl   $2 => Webserver vars prefix name
dnl   $3 => File which should be present
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_WS_DIR],
  [
    tempval=""
    AC_ARG_WITH(
      [$1],
      [  --with-$1=DIR           Location of $1 source dir ],
      [
        case "${withval}" in
          ""|"yes"|"YES"|"true"|"TRUE")
          ;;
          "no"|"NO"|"false"|"FALSE")
          AC_MSG_ERROR(valid apr source dir location required)
          ;;
          *)
          tempval="${withval}"

          if ${TEST} ! -d ${tempval} ; then
            AC_MSG_ERROR(Not a directory: ${tempval})
          fi

          if ${TEST} ! -f ${tempval}/$3; then
            AC_MSG_ERROR(can't locate ${tempval}/$3)
          fi

          if ${TEST} ! -z "$tempval" ; then
            $2_BUILD="true"
            $2_CFLAGS="-I ${tempval}/include"
            $2_DIR=${tempval}
            $2_HOME="${tempval}"
            $2_INCDIR="${tempval}/include"
            $2_LDFLAGS=""
            $2_LIBDIR=""
            AC_MSG_RESULT($1_DIR)
          fi
          ;;
        esac
      ])

      unset tempval
  ])


dnl --------------------------------------------------------------------------
dnl JK_WS_INCDIR
dnl   Set the WebServer include dir.
dnl   $1 => Webserver name
dnl   $2 => Webserver vars prefix name
dnl   $3 => File which should be present
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_WS_INCDIR],
  [
    tempval=""
    AC_ARG_WITH(
      [$1-include],
      [  --with-$1-include=DIR   Location of $1 include dir ],
      [  
        case "${withval}" in
          ""|"yes"|"YES"|"true"|"TRUE")
          ;;
          "no"|"NO"|"false"|"FALSE")
          AC_MSG_ERROR(valid apr include dir location required)
          ;;
          *)
          tempval="${withval}"
          if ${TEST} ! -d ${tempval} ; then
            AC_MSG_ERROR(Not a directory: ${tempval})
          fi

          if ${TEST} ! -f ${tempval}/$3; then
            AC_MSG_ERROR(can't locate ${tempval}/$3)
          fi

          if ${TEST} ! -z "$tempval" ; then
            $1_BUILD=""
            $1_CFLAGS="-I${tempval}"
            $1_CLEAN=""
            $1_DIR=""
            $1_INCDIR=${tempval}
            AC_MSG_RESULT($1_INCL)
          fi
          ;;
        esac
      ])

      unset tempval
  ])


dnl --------------------------------------------------------------------------
dnl JK_WS_LIBDIR
dnl   Set the WebServer library dir.
dnl   $1 => Webserver name
dnl   $2 => Webserver vars prefix name
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_WS_LIBDIR],
  [
    tempval=""
    AC_ARG_WITH(
      [$1-lib],
      [  --with-$1-lib=DIR       Location of $1 lib dir ],
      [
        case "${withval}" in
          ""|"yes"|"YES"|"true"|"TRUE")
          ;;
          "no"|"NO"|"false"|"FALSE")
          AC_MSG_ERROR(valid apr lib dir location required)
          ;;
        *)
          tempval="${withval}"

          if ${TEST} ! -d ${tempval} ; then
            AC_MSG_ERROR(Not a directory: ${tempval})
          fi

          if ${TEST} ! -z "$tempval" ; then
            $2_BUILD=""
            $2_CLEAN=""
            $2_DIR=""
            $2_LIBDIR=${tempval}
            $2_LDFLAGS=""
            AC_MSG_RESULT($1_LIB)
          fi

          ;;
          esac
      ])

      unset tempval
  ])

dnl vi:set sts=2 sw=2 autoindent:

