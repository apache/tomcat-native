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
dnl JK_WS_DIR
dnl   Set the WebServer source dir.
dnl   $1 => Webserver name
dnl   $2 => Webserver vars prefix name
dnl   $3 => File which should be present
dnl   $4 => Server specific source directory 
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
          AC_MSG_ERROR(valid $1 source dir location required)
          ;;
          "no"|"NO"|"false"|"FALSE")
          AC_MSG_ERROR(Don't use with/without $1 if you don't have $1)
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
	    if ${TEST} -d ${withval}/include ; then
	      $2_INCL="-I${tempval}/include"
              $2_INCDIR="${tempval}/include"
	    fi
	    if ${TEST} -d ${withval}/src/include ; then
	      # read osdir from the existing apache.
	      osdir=`${GREP} '^OSDIR=' ${withval}/src/Makefile.config | ${SED} -e 's:^OSDIR=.*/os:os:'`
	      if ${TEST} -z "${osdir}" ; then
	        osdir=os/unix
	      fi
	      $2_INCL="-I${tempval}/src/include -I${withval}/src/${osdir}"
              $2_INCDIR="${tempval}/src/include"
	    fi
	    if ${TEST} -d ${tempval}/srclib/apr ; then
	      # Apache 2 contains apr.
	      if ${TEST} ! -f ${tempval}/srclib/apr/config.status ; then
                AC_MSG_ERROR(configure Apache2 before mod_jk2)
	      fi
	      osdir=`${GREP} @OSDIR@ ${tempval}/srclib/apr/config.status | sed 's:s,@OSDIR@,::' | sed 's:,;t t::'`
	      $2_INCL="-I${tempval}/include -I${withval}/os/${osdir}"
	      $2_LIBEXEC=`${GREP} "^exp_libexecdir =" ${tempval}/build/config_vars.mk | sed 's:exp_libexecdir = ::'`
	      LIBTOOL=${tempval}/srclib/apr/libtool
	      APR_INCDIR=-I${tempval}/srclib/apr/include
	      APR_CFLAGS=`${tempval}/srclib/apr/apr-config --cflags`
	      APR_UTIL_INCDIR=-I${tempval}/srclib/apr-util/include
	      APR_LIBDIR_LA=`${tempval}/srclib/apr/apr-config --apr-la-file`

	      AC_SUBST(APR_INCDIR)
	      AC_SUBST(APR_CFLAGS)
	      AC_SUBST(APR_INCDIR)
	      AC_SUBST(APR_UTIL_INCDIR)
	    fi
            $2_LDFLAGS=""
            $2_LIBDIR=""
	    WEBSERVERS="${WEBSERVERS} $4"

	    AC_SUBST($2_BUILD)
	    AC_SUBST($2_CFLAGS)
	    AC_SUBST($2_DIR)
	    AC_SUBST($2_HOME)
	    AC_SUBST($2_INCL)
	    AC_SUBST($2_INCDIR)
	    AC_SUBST($2_LDFLAGS)
	    AC_SUBST($2_LIBDIR)

          fi
          ;;
        esac
      ])

      if ${TEST} -z "$tempval" ; then
        AC_MSG_RESULT(not provided)
      else	
        AC_MSG_RESULT(${tempval})
      fi

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
          AC_MSG_ERROR(valid $1 include dir location required)
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
            $2_BUILD=""
            $2_CFLAGS="-I${tempval}"
            $2_CLEAN=""
            $2_DIR=""
            $2_INCDIR=${tempval}
            AC_MSG_RESULT($2_INCDIR)
	    
	    AC_SUBST($2_BUILD)
	    AC_SUBST($2_CFLAGS)
	    AC_SUBST($2_CLEAN)
	    AC_SUBST($2_DIR)
	    AC_SUBST($2_INCDIR)
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
          AC_MSG_ERROR(valid $1 lib directory location required)
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
            AC_MSG_RESULT($2_LIBDIR)

	    AC_SUBST($2_BUILD)
	    AC_SUBST($2_CLEAN)
	    AC_SUBST($2_DIR)
	    AC_SUBST($2_LIBDIR)
	    AC_SUBST($2_LDFLAGS)
          fi

          ;;
          esac
      ])

      unset tempval
  ])

dnl vi:set sts=2 sw=2 autoindent:

