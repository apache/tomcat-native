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
dnl WA_ANT
dnl   Locate Apache ANT
dnl   $1 => Environment variable where the ANT binary? name will be stored
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [WA_ANT],
  [
    wa_ant_enabled=""
    wa_ant_tempval=""
    AC_MSG_CHECKING([if ant is enabled])
    AC_ARG_WITH(
      [ant],
      [  --with-ant[[=ant]]        the Apache Ant tool to use],
      [
        case "${withval}" in
        ""|"yes"|"YES"|"true"|"TRUE")
          AC_MSG_RESULT([yes])
          wa_ant_enabled="yes"
          wa_ant_tempval=""
          ;;
        "no"|"NO"|"false"|"FALSE")
          AC_MSG_RESULT([no])
          wa_ant_enabled="no"
          wa_ant_tempval=""
          ;;
        *)
          AC_MSG_RESULT([yes (${withval})])
          wa_ant_enabled="yes"
          WA_PATH_PROG([wa_ant_tempval],[${withval}],[ant])
          if test -z "${wa_ant_tempval}" ; then
            AC_MSG_ERROR([${withval} is invalid])
          fi
          ;;
        esac
      ],[
        AC_MSG_RESULT([guessing])
      ])

    if test "${wa_ant_enabled}" = "no" ; then
      wa_ant_tempval=""
    else
      if test -z "${wa_ant_tempval}" ; then
        WA_PATH_PROG([wa_ant_tempval],[ant],[ant])
      fi
      if test -z "${wa_ant_tempval}" ; then
        WA_PATH_PROG([wa_ant_tempval],[ant.sh],[ant.sh])
      fi
      if test -z "${wa_ant_tempval}" ; then
        if test "${wa_ant_enabled}" = "yes" ; then
          AC_MSG_ERROR([apache ant cannot be found])
          exit 1
        fi
      fi
    fi

    if test -n "${wa_ant_tempval}" ; then
      AC_MSG_CHECKING([if ${wa_ant_tempval} is working])

      wa_ant_enabled=`${wa_ant_tempval} -version`
      wa_ant_enabled=`echo ${wa_ant_enabled} | sed 's/^Apache Ant/Ant/g'`
      wa_ant_enabled=`echo ${wa_ant_enabled} | grep "^Ant version"`
      wa_ant_enabled=`echo ${wa_ant_enabled} | cut -d\  -f3`
      if test -z "${wa_ant_enabled}" ; then
        WA_ERROR([ant misconfigured, reconfigure with --without-ant])
      else
        AC_MSG_RESULT([${wa_ant_enabled}])
      fi
    fi
    
    $1="${wa_ant_tempval}"
    unset wa_ant_tempval
    unset wa_ant_enabled
  ])
