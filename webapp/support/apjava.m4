dnl =========================================================================
dnl
dnl                 The Apache Software License,  Version 1.1
dnl
dnl          Copyright (c) 1999-2001 The Apache Software Foundation.
dnl                           All rights reserved.
dnl
dnl =========================================================================
dnl
dnl Redistribution and use in source and binary forms,  with or without modi-
dnl fication, are permitted provided that the following conditions are met:
dnl
dnl 1. Redistributions of source code  must retain the above copyright notice
dnl    notice, this list of conditions and the following disclaimer.
dnl
dnl 2. Redistributions  in binary  form  must  reproduce the  above copyright
dnl    notice,  this list of conditions  and the following  disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl
dnl 3. The end-user documentation  included with the redistribution,  if any,
dnl    must include the following acknowlegement:
dnl
dnl       "This product includes  software developed  by the Apache  Software
dnl        Foundation <http://www.apache.org/>."
dnl
dnl    Alternately, this acknowlegement may appear in the software itself, if
dnl    and wherever such third-party acknowlegements normally appear.
dnl
dnl 4. The names "The Jakarta Project",  "Apache WebApp Module",  and "Apache
dnl    Software Foundation"  must not be used to endorse or promote  products
dnl    derived  from this  software  without  prior  written  permission. For
dnl    written permission, please contact <apache@apache.org>.
dnl
dnl 5. Products derived from this software may not be called "Apache" nor may
dnl    "Apache" appear in their names without prior written permission of the
dnl    Apache Software Foundation.
dnl
dnl THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES
dnl INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY
dnl AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL
dnl THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY
dnl DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL
dnl DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS
dnl OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION)
dnl HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT,
dnl STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
dnl ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE
dnl POSSIBILITY OF SUCH DAMAGE.
dnl
dnl =========================================================================
dnl
dnl This software  consists of voluntary  contributions made  by many indivi-
dnl duals on behalf of the  Apache Software Foundation.  For more information
dnl on the Apache Software Foundation, please see <http://www.apache.org/>.
dnl
dnl =========================================================================

dnl --------------------------------------------------------------------------
dnl Author Pier Fumagalli <pier.fumagalli@sun.com>
dnl Version $Id$
dnl --------------------------------------------------------------------------

dnl --------------------------------------------------------------------------
dnl JAVA_INIT
dnl   Check if --enable-java was specified and discovers JAVA_HOME.
dnl --------------------------------------------------------------------------
AC_DEFUN([JAVA_INIT],[
  AC_MSG_CHECKING([for java support])
  AC_ARG_ENABLE(java,
  [  --enable-java[=JAVA_HOME]  enable Java compilation (if JAVA_HOME is not
                          specified its value will be inherited from the
                          JAVA_HOME environment variable).],
  [
    case "${withval}" in
    yes|YES|true|TRUE)
      JAVA_ENABLE="TRUE"
      ;;
    *)
      JAVA_ENABLE="TRUE"
      JAVA_HOME="${withval}"
      ;;
    esac

    AC_MSG_RESULT([yes])

    if ${TEST} ! -z "${JAVA_HOME}"
    then
      LOCAL_RESOLVEDIR(JAVA_HOME,${JAVA_HOME},[java home directory])
      AC_MSG_RESULT([error])
      AC_MSG_ERROR([java home not specified and not found in environment])
    fi

  ],[
    JAVA_ENABLE="FALSE"
    AC_MSG_RESULT([no])
  ])
  AC_SUBST(JAVA_ENABLE)
  AC_SUBST(JAVA_HOME)
])

dnl --------------------------------------------------------------------------
dnl JAVA_JAVAC
dnl   Checks wether we can find and use a Java Compiler or not. Exports the
dnl   binary name in the JAVAC environment variable. Compilation flags are
dnl   retrieved from the JAVACFLAGS environment variable and checked.
dnl --------------------------------------------------------------------------
AC_DEFUN([JAVA_JAVAC],[
  if ${TEST} -z "${JAVA_HOME}"
  then
    JAVAC=""
    JAVACFLAGS=""
  else
    LOCAL_CHECK_PROG(JAVAC,javac,"${JAVA_HOME}/bin")

    AC_CACHE_CHECK([wether the Java compiler (${JAVAC}) works],
      ap_cv_prog_javac_works,[
      echo "public class Test {}" > Test.java
      ${JAVAC} ${JAVACFLAGS} Test.java > /dev/null 2>&1
      if ${TEST} $? -eq 0
      then
        rm -f Test.java Test.class
        ap_cv_prog_javac_works=yes
      else
        rm -f Test.java Test.class
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([${JAVAC} cannot compile])
      fi
    ])
  fi
  AC_SUBST(JAVAC)
  AC_SUBST(JAVACFLAGS)
])

dnl --------------------------------------------------------------------------
dnl JAVA_JAR
dnl   Checks wether we can find and use a Java Archieve Tool (JAR). Exports
dnl   the binary name in the JAR environment variable.
dnl --------------------------------------------------------------------------
AC_DEFUN([JAVA_JAR],[
  if ${TEST} -z "${JAVA_HOME}"
  then
    JAR=""
  else
    LOCAL_CHECK_PROG(JAR,jar,"${JAVA_HOME}/bin")
  fi
  AC_SUBST(JAR)
])

dnl --------------------------------------------------------------------------
dnl JAVA_JAVADOC
dnl   Checks wether we can find and use a Java Documentation Tool (JAVADOC).
dnl   Exports the binary name in the JAR environment variable.
dnl --------------------------------------------------------------------------
AC_DEFUN([JAVA_JAVADOC],[
  if ${TEST} -z "${JAVA_HOME}"
  then
    JAVADOC=""
  else
    LOCAL_CHECK_PROG(JAVADOC,javadoc,"${JAVA_HOME}/bin")
  fi
  AC_SUBST(JAVADOC)
])

