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
dnl Author Henri Gomez <hgomez@apache.org>
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
            APACHE$1_LIBEXEC="`${APXS$1} -q LIBEXECDIR`"
            APACHE$1_CC="`${APXS$1} -q CC`"

            dnl test apache version
            APA=`${GREP} STANDARD20 ${APXS$1}`

            dnl check if we have an apxs for Apache 1.3 or 2.0
            if ${TEST} -z "$APA" ; then
	      if ${TEST} ! -z "$1" ; then
                AC_MSG_ERROR(Do not use --with-apxs$1 but --with-apxs)
	      fi
              WEBSERVERS="${WEBSERVERS} server/apache13"
              RWEBSERVER="apache-1.3"
              APXS$1_CFLAGS="`${APXS$1} -q CFLAGS`"
              APXS$1_CPPFLAGS=""
            else
	      if ${TEST} -z "$1" ; then
                AC_MSG_ERROR(Do not use --with-apxs but --with-apxs2)
	      fi
              WEBSERVERS="${WEBSERVERS} server/apache2"
              RWEBSERVER="apache-2.0"
              APACHE2_CONFIG_VARS=${apache_dir}/build/config_vars.mk
              JK_CHANNEL_APR_SOCKET="\${JK}jk_channel_apr_socket\${OEXT}"
              JK_POOL_APR="\${JK}jk_pool_apr\${OEXT}"
              HAS_APR="-DHAS_APR"
              APXS$1_CFLAGS="`${APXS$1} -q CFLAGS` `${APXS$1} -q EXTRA_CFLAGS`"
              APXS$1_CPPFLAGS="`${APXS$1} -q EXTRA_CPPFLAGS`"
              APR_INCDIR="-I`${APXS$1} -q APR_INCLUDEDIR`"
			  APR_UTIL_INCDIR="-I`${APXS$1} -q APU_INCLUDEDIR`"
              APACHE2_LIBDIR="`${APXS$1} -q LIBDIR`"
              LIBTOOL=`${APXS$1} -q LIBTOOL`
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
  AC_SUBST(APACHE$1_LIBEXEC)
  AC_SUBST(APACHE$1_LIBDIR)
  AC_SUBST(APACHE$1_CC)
  AC_SUBST(APXS$1_LDFLAGS)

])

dnl vi:set sts=2 sw=2 autoindent:

