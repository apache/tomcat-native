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
dnl Author Pier Fumagalli <pier.fumagalli@eng.sun.com>
dnl Version $Id$
dnl --------------------------------------------------------------------------

AC_DEFUN(LOCAL_INIT,[
  AC_MSG_RESULT([])
  AC_MSG_RESULT([Initializing])
  ac_t="    "
  AC_PATH_PROG(TEST,test,${PATH})
  AC_PATH_PROG(TRUE,true,${PATH})
  AC_PATH_PROG(ECHO,echo,${PATH})
  AC_PATH_PROG(GREP,grep,${PATH})
  AC_PATH_PROG(CAT,cat,${PATH})
  AC_PATH_PROG(SED,sed,${PATH})
  AC_PATH_PROG(RM,rm,${PATH})
  AC_SUBST(TEST)
  AC_SUBST(TRUE)
  AC_SUBST(ECHO)
  AC_SUBST(GREP)
  AC_SUBST(CAT)
  AC_SUBST(SED)
  AC_SUBST(RM)
])

AC_DEFUN(LOCAL_HEADER,[
  ${ECHO} ""
  ${ECHO} "${1}" 1>&2
])

AC_DEFUN(LOCAL_FILTEREXEC,[
  ${ECHO} "  Invoking: ${1}"
  ${ECHO} "-1" > retvalue.tmp
  {
    ${1}
    ${ECHO} retvalue ${?}
  }|{
    ret=0
    while ${TRUE}
    do
      read first line
      if ${TEST} ! "${?}" -eq "0"
      then
        break
      else
        if ${TEST} "${first}" = "retvalue"
        then
          ret="${line}"
        else
          ${ECHO} "    ${2}: ${first} ${line}"
        fi
      fi
    done
    unset first
    unset line
    echo "${ret}" > retvalue.tmp
  }
  ret=`${CAT} retvalue.tmp`
  ${RM} retvalue.tmp
  ${ECHO} "  Execution of ${1} returned ${ret}"
])

