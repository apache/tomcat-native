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
