dnl  =========================================================================
dnl
dnl                  The Apache Software License,  Version 1.1
dnl
dnl           Copyright (c) 1   -2001 The Apache Software Foundation.
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
dnl JK_APXS
dnl
dnl Get APXS to be used, determine if Apache 1.3 or 2.0 are target
dnl $1 => blank/2 if you want to detect Apache 1.3 & 2.0 
dnl $2 => comment for --with-apxs
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_APXS],
  [
    tempval=""
    AC_ARG_WITH(apxs$1,
    [  --with-apxs$1[=FILE]      $2],
    [
      case "${withval}" in 
        y | yes | true) find_apxs=true ;;
        n | no | false) find_apxs=false ;;
        *) find_apxs=false ;;
      esac

      if ${TEST} ${find_apxs} ; then    
        AC_MSG_RESULT([need to check for Perl first, apxs depends on it...])
        AC_PATH_PROG(PERL,perl,$PATH)dnl
    
        if ${TEST} ${find_apxs} ; then
            APXS$1=${withval}
        else
            AC_PATH_PROG(APXS$1,apxs$1,$PATH)dnl
        fi
    
		use_apxs$1=true;
		
        if ${TEST} -n "${APXS$1}" ; then
            dnl Seems that we have it, but have to check if it is OK first        
            if ${TEST} ! -x "${APXS$1}" ; then
                AC_MSG_ERROR(Invalid location for apxs: '${APXS$1}')
            fi
            
            ${APXS$1} -q PREFIX >/dev/null 2>/dev/null || apxs_support=false
    
            if ${TEST} "${apxs_support}" = "false" ; then
                AC_MSG_RESULT(could not find ${APXS$1})
                AC_MSG_ERROR(You must specify a valid --with-apxs$1 path)
            fi

            dnl apache_dir and apache_include are also needed.
            APACHE$1_HOME=`${APXS$1} -q PREFIX`
            APACHE$1_INCL="-I`${APXS$1} -q INCLUDEDIR`"
            APACHE$1_INCDIR="`${APXS$1} -q INCLUDEDIR`"
            APACHE$1_LIBDIR="`${APXS$1} -q LIBEXECDIR`"

            dnl test apache version
            APA=`${GREP} STANDARD20 ${APXS$1}`

            dnl check if we have an apxs for Apache 1.3 or 2.0
            if ${TEST} -z "$APA" ; then
              WEBSERVERS="${WEBSERVERS} server/apache13"
              RWEBSERVER="apache-1.3"
              APXS$1_CFLAGS="`${APXS$1} -q CFLAGS`"
              APXS$1_CPPFLAGS=""
            else
              WEBSERVERS="${WEBSERVERS} server/apache2"
              RWEBSERVER="apache-2.0"
              APACHE2_CONFIG_VARS=${apache_dir}/build/config_vars.mk
              JK_CHANNEL_APR_SOCKET="\${JK}jk_channel_apr_socket\${OEXT}"
              JK_POOL_APR="\${JK}jk_pool_apr\${OEXT}"
              HAS_APR="-DHAS_APR"
              APXS$1_CFLAGS="`${APXS$1} -q CFLAGS` `${APXS$1} -q EXTRA_CFLAGS`"
              APXS$1_CPPFLAGS="`${APXS$1} -q EXTRA_CPPFLAGS`"
            fi
            
            AC_MSG_RESULT([building connector for \"$RWEBSERVER\"])
        fi

      fi
  ],
  [
	  AC_MSG_RESULT(no apxs$1 given)
  ])

  unset tempval

  AC_SUBST(APXS$1)
  AC_SUBST(APXS$1_CFLAGS)
  AC_SUBST(APACHE$1_CONFIG_VARS)
  AC_SUBST(APXS$1_CPPFLAGS)
  AC_SUBST(APACHE$1_DIR)
  AC_SUBST(APACHE$1_HOME)
  AC_SUBST(APACHE$1_INCDIR)
  AC_SUBST(APACHE$1_INCL)
  AC_SUBST(APACHE$1_LIBDIR)
  AC_SUBST(APXS$1_LDFLAGS)

])

dnl vi:set sts=2 sw=2 autoindent:

