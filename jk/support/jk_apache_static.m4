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
dnl Author Henri Gomez <hgomez@apache.org>
dnl
dnl Inspired by Pier works on webapp m4 macros :)
dnl 
dnl Version $Id$
dnl --------------------------------------------------------------------------

dnl Apache-2.0 needs the os subdirectory to include os.h
dnl this include is copy from os/config.m4
sinclude(../../common/build/os_apache.m4)

dnl --------------------------------------------------------------------------
dnl JK_APACHE_STATIC
dnl   Set the APACHE 1.3/2.0 source dir.
dnl   $1 => apache source dir to detect ("", 2)
dnl   $2 => apache 1.3 build dir 
dnl   $3 => apache 2.0 build dir
dnl
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_APACHE_STATIC],
  [
    tempval=""

    AC_ARG_WITH(
      [apache$1],
      [  --with-apache$1=DIR  Location of Apache$2 source dir],
      [
        if ${TEST} ${use_apxs$1} ; then
          AC_MSG_ERROR([Sorry cannot use --with-apxs= and --with-apache= together, please choose one])
        fi

        AC_MSG_CHECKING([for Apache source directory (assume static build)])
        
        if ${TEST} -n "${withval}" && ${TEST} -d "${withval}" ; then

          if ${TEST} -d "${withval}/src" ; then
            # handle the case where people use relative paths to 
            # the apache source directory by pre-pending the current
            # build directory to the path. there are probably 
            # errors with this if configure is run while in a 
            # different directory than what you are in at the time
            if ${TEST} -n "`${ECHO} ${withval}|${GREP} \"^\.\.\"`" ; then
              withval=`pwd`/${withval}
            fi

            APACHE$1_DIR=${withval}
            use_static="true"
            AC_MSG_RESULT(${APACHE$1_DIR})
        
            AC_MSG_CHECKING(for Apache include directory)

            if ${TEST} -d "${withval}/src/include" ; then
              # read osdir from the existing apache.
              osdir=`${GREP} '^OSDIR=' ${withval}/src/Makefile.config | ${SED} -e 's:^OSDIR=.*/os:os:'`

              if ${TEST} -z "${osdir}" ; then
                osdir=os/unix
              fi

              APACHE$1_DIR=${withval}
              APACHE$1_HOME=${withval}
              APACHE$1_INCL="-I${withval}/src/include -I${withval}/src/${osdir}"
              EXTRA_CFLAGS=""
              EXTRA_CPPFLAGS=""
              REPORTED_SERVER="apache-1.3"
              SERVER_DIR="$3"
              use_static="true"
              use_apache13="true"
              AC_MSG_RESULT([${APACHE$1_INCL}, version 1.3])
            else
              AC_MSG_ERROR([Sorry Apache 1.2.x is no longer supported.])
            fi

          else

            if ${TEST} -d "${withval}/include" ; then
              # osdir for Apache20.
              APACHE$1_DIR=${withval}
              APACHE$1_HOME=${withval}
              APACHE$1_INCL="-I${withval}/include -I${withval}/srclib/apr/include -I${withval}/os/${OS_APACHE_DIR} -I${withval}/srclib/apr-util/include"
              EXTRA_CFLAGS=""
              EXTRA_CPPFLAGS=""
              REPORTED_SERVER="apache-2.0"
              SERVER_DIR="$3"
              use_static="true"
              use_apache2="true"
              APACHE$1_INCL="-I${withval}/include -I${withval}/srclib/apr/include -I${withval}/os/${OS_APACHE_DIR} -I${withval}/srclib/apr-util/include"
              AC_MSG_RESULT(${APACHE$1_DIR})

              
              JK_CHANNEL_APR_SOCKET="\${JK}jk_channel_apr_socket\${OEXT}"
              JK_POOL_APR="\${JK}jk_pool_apr\${OEXT}"
              HAS_APR="-DHAS_APR"
           fi
        fi
    fi

    dnl Make sure we have a result.
    if ${TEST} -z "$WEBSERVER" ; then
        AC_MSG_ERROR([Directory $apache_dir is not a valid Apache source distribution])
    fi

# VT: Now, which one I'm supposed to use? Let's figure it out later

    configure_apache=true
    configure_src=true
    
    AC_MSG_RESULT([building connector for \"$WEBSERVER\"])
],
[
	AC_MSG_RESULT(no apache$1 dir given)
])

dnl vi:set sts=2 sw=2 autoindent:
