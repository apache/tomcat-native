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
dnl WA_VERSION
dnl   Retrieve the version of the WebApp module
dnl   $1 => The variable name where the version number will be stored.
dnl   $2 => A working C compiler.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_VERSION],
  [
    cat > ${TGT_DIR}/wa_version.c << EOF
#include "${SRC_DIR}/include/wa_version.h"
#include "stdio.h"

int main(void) [ {
  printf(WA_VERSION "\n");
  exit(0);
} ]
EOF
    AC_MSG_CHECKING([for webapp version])
    $2 ${TGT_DIR}/wa_version.c -o ${TGT_DIR}/wa_version
    if test "$?" != "0" ; then
      AC_MSG_ERROR([compiler didn't run successfully])
    fi
    $1=`${TGT_DIR}/wa_version`
    if test "$?" != "0" ; then
      AC_MSG_ERROR([cannot execute wa_version.o])
    fi
    if test -z "${$1}" ; then
      AC_MSG_ERROR([empty version number])
    fi
    rm -f "${TGT_DIR}/wa_version"
    rm -f "${TGT_DIR}/wa_version.c"
    AC_MSG_RESULT([${$1}])
    
  ])
