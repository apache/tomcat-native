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
dnl JK_APR_THREADS
dnl   Configure APR threading for use with --with-apr.
dnl   Result goes into APR_CONFIGURE_ARGS
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_APR_THREADS],
  [
    AC_ARG_ENABLE(
      [apr-threads],
      [  --enable-apr-threads        Configure APR threading for use with --with-apr ],
      [
        case "${enableval}" in
          ""|"yes"|"YES"|"true"|"TRUE")
            APR_CONFIGURE_ARGS="--enable-threads ${APR_CONFIGURE_ARGS}"
          ;;
          "no"|"NO"|"false"|"FALSE")
            APR_CONFIGURE_ARGS="--disable-threads ${APR_CONFIGURE_ARGS}"
          ;;
        *)
          APR_CONFIGURE_ARGS="--enable-threads=${enableval} ${APR_CONFIGURE_ARGS}"
         esac
      ])
  ])

dnl --------------------------------------------------------------------------
dnl JK_APR
dnl   Set the APR source dir.
dnl   $1 => File which should be present
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_APR],
  [
    tempval=""
    AC_ARG_WITH(
      [apr],
      [  --with-apr=DIR           Location of APR source dir ],
      [
        case "${withval}" in
          ""|"yes"|"YES"|"true"|"TRUE")
            AC_MSG_ERROR(valid apr source dir location required)
          ;;
          "no"|"NO"|"false"|"FALSE")
            AC_MSG_ERROR(valid apr source dir location required)
          ;;
        *)
          tempval="${withval}"

          if ${TEST} ! -d ${tempval} ; then
            AC_MSG_ERROR(Not a directory: ${tempval})
          fi

          if ${TEST} ! -f ${tempval}/$1; then
            AC_MSG_ERROR(can't locate ${tempval}/$1)
          fi

          if ${TEST} ! -z "$tempval" ; then
            APR_BUILD="apr-build"
            APR_CFLAGS="-I ${tempval}/include"
            APR_CLEAN="apr-clean"
            APR_DIR=${tempval}
            APR_INCDIR="${tempval}/include"
            AC_MSG_RESULT(configuring apr...)
            tempret="0"
            JK_EXEC(
              [tempret],
              [${SHELL} ./configure --prefix=${APR_DIR} --with-installbuilddir=${APR_DIR}/instbuild --disable-shared ${APR_CONFIGURE_ARGS}],
              [apr],
              [${APR_DIR}])
            if ${TEST} "${tempret}" = "0"; then
              AC_MSG_RESULT(apr configure ok)
            else
              AC_MSG_ERROR(apr configure failed with ${tempret})
            fi
            JK_APR_LIBNAME(apr_libname,${APR_DIR})
            APR_LDFLAGS="${APR_DIR}/lib/${apr_libname}"
            APR_LIBDIR=""
			use_apr=true
            COMMON_APR_OBJECTS="\${COMMON_APR_OBJECTS}"
          fi
          ;;
        esac
      ])

      unset tempret
      unset tempval
      unset apr_libname
  ])

dnl --------------------------------------------------------------------------
dnl JK_APR_UTIL
dnl   Set the APR-UTIL source dir.
dnl   $1 => File which should be present
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_APR_UTIL],
  [
    tempval=""
    AC_ARG_WITH(
      [apr-util],
      [  --with-apr-util=DIR      Location of APR-UTIL source dir ],
      [
        case "${withval}" in
          ""|"yes"|"YES"|"true"|"TRUE")
            AC_MSG_ERROR(valid apr-util source dir location required)
          ;;
          "no"|"NO"|"false"|"FALSE")
            AC_MSG_ERROR(valid apr-util source dir location required)
          ;;
        *)
          tempval="${withval}"

          if ${TEST} ! -d ${tempval} ; then
            AC_MSG_ERROR(Not a directory: ${tempval})
          fi

          if ${TEST} ! -f ${tempval}/$1; then
            AC_MSG_ERROR(can't locate ${tempval}/$1)
          fi

          if ${TEST} -z "${APR_BUILD}"; then
            AC_MSG_ERROR([--with-apr and --with-apr-util must be used together])
          fi

          if ${TEST} ! -z "$tempval" ; then
            APR_UTIL_DIR=${tempval}
            APR_CFLAGS="${APR_CFLAGS} -I ${APR_UTIL_DIR}/include"
            APR_UTIL_INCDIR="${APR_UTIL_DIR}/include"
            AC_MSG_RESULT(configuring apr-util...)
            tempret="0"
            JK_EXEC(
              [tempret],
              [${SHELL} ./configure --prefix=${APR_UTIL_DIR} --with-apr=${APR_DIR}],
              [apr-util],
              [${APR_UTIL_DIR}])
            if ${TEST} "${tempret}" = "0"; then
              AC_MSG_RESULT(apr-util configure ok)
            else
              AC_MSG_ERROR(apr-util configure failed with ${tempret})
            fi
            JK_APR_UTIL_LIBNAME(apr_util_libname,${APR_UTIL_DIR})
            APR_LDFLAGS="${APR_LDFLAGS} ${APR_UTIL_DIR}/lib/${apr_util_libname}"
            APR_UTIL_LIBDIR=""
			use_apr=true
            COMMON_APR_OBJECTS="\${COMMON_APR_OBJECTS}"
          fi
          ;;
        esac
      ])

      unset tempret
      unset tempval
      unset apr_util_libname
  ])


dnl --------------------------------------------------------------------------
dnl JK_APR_INCDIR
dnl   Set the APR include dir.
dnl   $1 => File which should be present
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_APR_INCDIR],
  [
    tempval=""
    AC_ARG_WITH(
      [apr-include],
      [  --with-apr-include=DIR   Location of APR include dir ],
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

          if ${TEST} ! -f ${tempval}/$1; then
            AC_MSG_ERROR(can't locate ${tempval}/$1)
          fi

          if ${TEST} ! -z "$tempval" ; then
            APR_BUILD=""
            APR_CFLAGS="-I${tempval}"
            APR_CLEAN=""
            APR_DIR=""
            APR_INCDIR=${tempval}
            COMMON_APR_OBJECTS="\${COMMON_APR_OBJECTS}"
			use_apr=true
          fi
          ;;

        esac
      ])

      unset tempval
  ])


dnl --------------------------------------------------------------------------
dnl JK_APR_LIBDIR
dnl   Set the APR library dir.
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_APR_LIBDIR],
  [
    tempval=""
    AC_ARG_WITH(
      [apr-lib],
      [  --with-apr-lib=DIR       Location of APR lib dir ],
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
              APR_BUILD=""
              APR_CLEAN=""
              APR_DIR=""
              APR_LIBDIR=${tempval}
              APR_LDFLAGS="`apr-config --link-ld` -L${tempval}"
              COMMON_APR_OBJECTS="\${COMMON_APR_OBJECTS}"
			  use_apr=true
            fi

            ;;
            esac
      ])

      unset tempval
  ])


dnl --------------------------------------------------------------------------
dnl JK_APR_LIBNAME
dnl   Retrieve the complete name of the library.
dnl   $1 => Environment variable name for the returned value
dnl   $2 => APR sources directory
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_APR_LIBNAME],
  [
    AC_MSG_CHECKING([for apr APR_LIBNAME])
    if ${TEST} ! -f "$2/apr-config" ; then
      AC_MSG_ERROR([cannot find apr-config file in $2])
    fi
    jk_apr_get_tempval=`$2/apr-config --link-libtool 2> /dev/null`
    if ${TEST} -z "${jk_apr_get_tempval}" ; then
      AC_MSG_ERROR([$2/apr-config --link-libtool failed])
    fi
    jk_apr_get_tempval=`basename ${jk_apr_get_tempval}`
    $1="${jk_apr_get_tempval}"
    AC_MSG_RESULT([${jk_apr_get_tempval}])
    unset jk_apr_get_tempval
  ])


dnl --------------------------------------------------------------------------
dnl JK_APR_UTIL_LIBNAME
dnl   Retrieve the complete name of the library.
dnl   $1 => Environment variable name for the returned value
dnl   $2 => APR_UTIL sources directory
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_APR_UTIL_LIBNAME],
  [
    AC_MSG_CHECKING([for apr-util APR_UTIL_LIBNAME])
    if ${TEST} ! -f "$2/apu-config" ; then
      AC_MSG_ERROR([cannot find apu-config file in $2])
    fi
    jk_apu_get_tempval=`$2/apu-config --link-libtool 2> /dev/null`
    if ${TEST} -z "${jk_apu_get_tempval}" ; then
      AC_MSG_ERROR([$2/apu-config --link-libtool failed])
    fi
    jk_apu_get_tempval=`basename ${jk_apu_get_tempval}`
    $1="${jk_apu_get_tempval}"
    AC_MSG_RESULT([${jk_apu_get_tempval}])
    unset jk_apu_get_tempval
  ])

dnl vi:set sts=2 sw=2 autoindent:

