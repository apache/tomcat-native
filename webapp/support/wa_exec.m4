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
dnl WA_EXEC
dnl   Execute a program filtering its output (pretty printing).
dnl
dnl   Parameters:
dnl     $1 => name of the variable containing the return value (error code).
dnl     $2 => name of the binary/script to invoke
dnl     $3 => message used for pretty printing output
dnl     $4 => the directory where the command must be executed
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_EXEC],
  [
    wa_exec_curdir="`pwd`"
    if test -d "$4" ; then
      cd "$4"
    else
      AC_MSG_ERROR([can't switch to directory $4]) 
    fi

    echo "  invoking \"$2\""
    echo "  in directory \"$4\""
    echo "-1" > retvalue.tmp

    set $2
    wa_exec_file=[$]1
    if test ! -x "${wa_exec_file}" ; then
      cd "${wa_exec_curdir}"
      AC_MSG_ERROR([cannot find or execute \"${wa_exec_file}\" in \"$4\"])
      exit 1
    fi
    unset wa_exec_file

    {
      $2
      echo "wa_exec_retvalue $?"
    } | {
      wa_exec_ret=0
      while true ; do
        read wa_exec_first wa_exec_line
        if test ! "$?" -eq "0" ; then
          break
        else
          if test "${wa_exec_first}" = "wa_exec_retvalue" ; then
            wa_exec_ret="${wa_exec_line}"
          else
            if test -n "${wa_exec_line}" ; then
             echo "    $3: ${wa_exec_first} ${wa_exec_line}"
            fi
          fi
        fi
      done
      echo "${wa_exec_ret}" > retvalue.tmp
      unset wa_exec_first
      unset wa_exec_line
      unset wa_exec_ret
    }

    $1="`cat retvalue.tmp`"
    rm -f retvalue.tmp
    echo "  execution of \"$2\""
    echo "  returned with value \"$1\""

    cd "${wa_exec_curdir}"
    unset wa_exec_curdir
  ])
