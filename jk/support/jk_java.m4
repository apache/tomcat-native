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
dnl JK_JDK
dnl
dnl Detection of JDK location and Java Platform (1.1, 1.2, 1.3, 1.4)
dnl result goes in JAVA_HOME / JAVA_PLATFORM (1 -> 1.1, 2 -> 1.2 and higher)
dnl 
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_JNI],
  [
    AC_ARG_WITH(jni,
      [  --with-jni               Build jni support],
      [
		case "${withval}" in
		  y | yes | true) use_jni=true ;;
		  n | no | false) use_jni=false ;;
	    *) use_jni=true ;;
	      esac

		if ${TEST} ${use_jni} ; then
		  HAVE_JNI="-DHAVE_JNI"
		  JNI_BUILD="jni-build"
		fi
      ])
  ])

AC_DEFUN(
  [JK_JDK],
  [
    if ${TEST} "${use_jni}" = "true"; then
      tempval=""
      AC_MSG_CHECKING([for JDK location (please wait)])
      if ${TEST} -n "${JAVA_HOME}" ; then
        JAVA_HOME_ENV="${JAVA_HOME}"
      else
        JAVA_HOME_ENV=""
      fi

      JAVA_HOME=""
      JAVA_PLATFORM=""

      AC_ARG_WITH(
        [java-home],
        [  --with-java-home=DIR     Location of JDK directory.],
        [

        # This stuff works if the command line parameter --with-java-home was
        # specified, so it takes priority rightfully.
  
        tempval=${withval}

        if ${TEST} ! -d "${tempval}" ; then
            AC_MSG_ERROR(Not a directory: ${tempval})
        fi
  
        JAVA_HOME=${tempval}
        AC_MSG_RESULT(${JAVA_HOME})
      ],
      [
        # This works if the parameter was NOT specified, so it's a good time
        # to see what the enviroment says.
        # Since Sun uses JAVA_HOME a lot, we check it first and ignore the
        # JAVA_HOME, otherwise just use whatever JAVA_HOME was specified.

        if ${TEST} -n "${JAVA_HOME_ENV}" ; then
          JAVA_HOME=${JAVA_HOME_ENV}
          AC_MSG_RESULT(${JAVA_HOME_ENV} from environment)
        fi
      ])

      if ${TEST} -z "${JAVA_HOME}" ; then

        # Oh well, nobody set neither JAVA_HOME nor JAVA_HOME, have to guess
        # The following code is based on the code submitted by Henner Zeller
        # for ${srcdir}/src/scripts/package/rpm/ApacheJServ.spec
        # Two variables will be set as a result:
        #
        # JAVA_HOME
        # JAVA_PLATFORM
        AC_MSG_CHECKING([Try to guess JDK location])

        for JAVA_PREFIX in /usr/local /usr/local/lib /usr /usr/lib /opt /usr/java ; do

          for JAVA_PLATFORM in 4 3 2 1 ; do

            for subversion in .9 .8 .7 .6 .5 .4 .3 .2 .1 "" ; do

              for VARIANT in IBMJava2- java java- jdk jdk-; do
                GUESS="${JAVA_PREFIX}/${VARIANT}1.${JAVA_PLATFORM}${subversion}"
dnl             AC_MSG_CHECKING([${GUESS}])
                if ${TEST} -d "${GUESS}/bin" & ${TEST} -d "${GUESS}/include" ; then
                  JAVA_HOME="${GUESS}"
                  AC_MSG_RESULT([${GUESS}])
                  break
                fi
              done

              if ${TEST} -n "${JAVA_HOME}" ; then
                break;
              fi

            done

            if ${TEST} -n "${JAVA_HOME}" ; then
              break;
            fi

          done

          if ${TEST} -n "${JAVA_HOME}" ; then
            break;
          fi

        done

        if ${TEST} ! -n "${JAVA_HOME}" ; then
          AC_MSG_ERROR(can't locate a valid JDK location)
        fi

      fi

      if ${TEST} -n "${JAVA_PLATFORM}"; then
        AC_MSG_RESULT(Java Platform detected - 1.${JAVA_PLATFORM})
      else
        AC_MSG_CHECKING(Java platform)
      fi

      AC_ARG_WITH(java-platform,
       [  --with-java-platform[=2] Force the Java platorm
                                   (value is 1 for 1.1.x or 2 for 1.2.x or greater)],
       [
          case "${withval}" in
            "1"|"2")
              JAVA_PLATFORM=${withval}
              ;;
            *)
              AC_MSG_ERROR(invalid java platform provided)
              ;;
          esac
       ],
       [
          if ${TEST} -n "${JAVA_PLATFORM}"; then
            AC_MSG_RESULT(Java Platform detected - 1.${JAVA_PLATFORM})
          else
            AC_MSG_CHECKING(Java platform)
          fi
       ])

       AC_MSG_RESULT(${JAVA_PLATFORM})

      unset tempval
    else
      # no jni, then make sure JAVA_HOME is not picked up from env
      JAVA_HOME=""
      JAVA_PLATFORM=""
    fi
  ])


AC_DEFUN(
  [JK_JDK_OS],
  [
    if ${TEST} "${use_jni}" = "true"; then
      tempval=""
      OS=""
      AC_ARG_WITH(os-type,
        [  --with-os-type[=SUBDIR]  Location of JDK os-type subdirectory.],
        [
          tempval=${withval}

          if ${TEST} ! -d "${JAVA_HOME}/${tempval}" ; then
            AC_MSG_ERROR(Not a directory: ${JAVA_HOME}/${tempval})
          fi

          OS = ${tempval}
        ],
        [   
          AC_MSG_CHECKING(os_type directory)
          if ${TEST} -f ${JAVA_HOME}/include/jni_md.h; then
            OS=""
          else
            for f in ${JAVA_HOME}/include/*/jni_md.h; do
              if ${TEST} -f $f; then
                OS=`dirname ${f}`
                OS=`basename ${OS}`
                echo " ${OS}"
              fi
            done
            if ${TEST} -z "${OS}"; then
              AC_MSG_RESULT(Cannot find jni_md.h in ${JAVA_HOME}/${OS})
              AC_MSG_ERROR(You should retry --with-os-type=SUBDIR)
            fi
          fi
        ])
    fi
  ])

dnl vi:set sts=2 sw=2 autoindent:

