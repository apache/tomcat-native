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
dnl Author Pier Fumagalli <pier.fumagalli@sun.com>
dnl Version $Id$
dnl --------------------------------------------------------------------------

dnl --------------------------------------------------------------------------
dnl LOCAL_INIT
dnl   Discover binaries required further on in the automake process and to
dnl   process makefiles. Those are TRUE, ECHO, GREP, CAT, SED and RM
dnl --------------------------------------------------------------------------
AC_DEFUN(LOCAL_INIT,[
  AC_PATH_PROG(TEST,"test","test",${PATH})
  LOCAL_CHECK_PROG(TRUE,true)
  LOCAL_CHECK_PROG(ECHO,echo)
  LOCAL_CHECK_PROG(GREP,grep)
  LOCAL_CHECK_PROG(CAT,cat)
  LOCAL_CHECK_PROG(SED,sed)
  LOCAL_CHECK_PROG(LN,ln)
  LOCAL_CHECK_PROG(RM,rm)
  AC_SUBST(TEST)
  AC_SUBST(TRUE)
  AC_SUBST(ECHO)
  AC_SUBST(GREP)
  AC_SUBST(CAT)
  AC_SUBST(SED)
  AC_SUBST(LN)
  AC_SUBST(RM)
])

dnl --------------------------------------------------------------------------
dnl LOCAL_CHECK_PROG
dnl   Check wether a program exists in the current PATH or not and fail if
dnl   this was not found.
dnl
dnl   Parameters:
dnl     $1 - name of the variable where the program name must be stored
dnl     $2 - binary name of the program to look for
dnl     $3 - Extra PATH to be added to the search
dnl --------------------------------------------------------------------------
AC_DEFUN(LOCAL_CHECK_PROG,[
  if ${TEST} -z "$3"
  then
    local_path="${PATH}"
  else
    local_path="$3:${PATH}"
  fi
  local_vnam="$1"
  unset $1
  AC_PATH_PROG($1,$2,"",${local_path})
  local_vval=`eval "echo \\$$local_vnam"`
  if ${TEST} -z "${local_vval}"
  then
    AC_MSG_ERROR([cannot find required binary \"$2\"])
  fi
  unset local_vnam
  unset local_vval
  unset local_path
])

dnl --------------------------------------------------------------------------
dnl LOCAL_HEADER
dnl   Output a header for a stage of the configure process
dnl
dnl   Parameters:
dnl     $1 - message to be printed in the header
dnl --------------------------------------------------------------------------
AC_DEFUN(LOCAL_HEADER,[
  ${ECHO} ""
  ${ECHO} "$1" 1>&2
])

AC_DEFUN(LOCAL_HELP,[
  AC_DIVERT_PUSH(AC_DIVERSION_NOTICE)
  ac_help="${ac_help}

[$1]"
  AC_DIVERT_POP()
])

dnl --------------------------------------------------------------------------
dnl LOCAL_FILTEREXEC
dnl   Execute a program filtering its output (pretty printing).
dnl
dnl   Parameters:
dnl     $1 - name of the variable containing the return value (error code).
dnl     $2 - name of the binary/script to invoke
dnl     $3 - message used for pretty printing output
dnl     $4 - the directory where the command must be executed
dnl --------------------------------------------------------------------------
AC_DEFUN(LOCAL_FILTEREXEC,[
  local_curdir="`pwd`"
  if ${TEST} -n "$4"
  then
    cd "$4"
  fi

  local_vnam="$1"
  ${ECHO} "  Invoking: \"$2\" in \"$4\""
  ${ECHO} "-1" > retvalue.tmp

  set local_file $2
  local_file=[$]2
  if ${TEST} ! -x "${local_file}"
  then
    AC_MSG_ERROR([cannot find or execute \"${local_file}\" in \"$4\"])
  fi

  {
    $2
    ${ECHO} retvalue $?
  }|{
    ret=0
    while ${TRUE}
    do
      read first line
      if ${TEST} ! "$?" -eq "0"
      then
        break
      else
        if ${TEST} "${first}" = "retvalue"
        then
          ret="${line}"
        else
          ${ECHO} "    $3: ${first} ${line}"
        fi
      fi
    done
    echo "${ret}" > retvalue.tmp
    unset first
    unset line
    unset ret
  }

  eval "$local_vnam=`${CAT} retvalue.tmp`"
  local_vval=`eval "echo \\$$local_vnam"`
  ${RM} retvalue.tmp
  ${ECHO} "  Execution of $2 returned ${local_vval}"

  cd "${local_curdir}"
  unset local_curdir
  unset local_vnam
  unset local_vval


])

dnl --------------------------------------------------------------------------
dnl LOCAL_RESOLVEDIR
dnl   Resolve the full path name of a directory.
dnl
dnl   Parameters:
dnl     $1 - name of the variable in which the full path name will be stored
dnl     $2 - directory name to resolve
dnl     $3 - message used for this check
dnl --------------------------------------------------------------------------
AC_DEFUN(LOCAL_RESOLVEDIR,[
  AC_MSG_CHECKING([for $3])
  if ${TEST} -d "$2"
  then
    curdir="`pwd`"
    cd "$2"
    newdir="`pwd`"
    eval "$1=${newdir}"
    AC_SUBST($1)
    AC_MSG_RESULT([${newdir}])
    cd "${curdir}"
    unset curdir
    unset newdir
  else
    AC_MSG_RESULT([error])
    AC_MSG_ERROR([Directory not found \"$2\"])
  fi
])
