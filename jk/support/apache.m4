dnl
dnl apache.m4: autoconf macro for Apache/apxs
dnl

dnl
dnl check for apxs.
dnl

AC_DEFUN(JTC_CHECK_APXS,[
WEBSERVER=""
apache_dir=""
apache_include=""
APXS="apxs"
AC_ARG_WITH(apxs,
[  --with-apxs[=FILE]      Build shared Apache module. FILE is the optional
                        pathname to the apxs tool; defaults to finding
			apxs in your PATH.],
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
            APXS=${withval}
        else
            AC_PATH_PROG(APXS,apxs,$PATH)dnl
        fi
    
        if ${TEST} -n "${APXS}" ; then
            dnl Seems that we have it, but have to check if it is OK first        
            if ${TEST} ! -x "${APXS}" ; then
                AC_MSG_ERROR(Invalid location for apxs: '${APXS}')
            fi
            
            $APXS -q PREFIX >/dev/null 2>/dev/null || apxs_support=false
    
            if ${TEST} "${apxs_support}" = "false" ; then
                AC_MSG_RESULT(could not find apxs)
                AC_MSG_ERROR(You must specify a valid --with-apxs path)
            fi

            dnl test apache version
            $RM -rf test
            $APXS -n test -g
            APA=`grep STANDARD20 test/mod_test.c`
            $RM -rf test
            if ${TEST} -z "$APA" ; then
                WEBSERVER="apache-1.3"
            else
                WEBSERVER="apache-2.0"
            fi
            AC_MSG_RESULT([building connector for \"$WEBSERVER\"])
    
            AC_SUBST(APXS)

            dnl apache_dir and apache_include are also needed.
	    apache_dir=`$APXS -q PREFIX`
	    apache_include="-I`$APXS -q INCLUDEDIR`"
        fi
    fi
],
[
	AC_MSG_RESULT(no apxs given)
])
])dnl

dnl
dnl check for apache (static link).
dnl

AC_DEFUN(JTC_CHECK_APACHE,[

dnl it is copied from the configure of JServ ;=)
dnl and adapted. 

apache_dir_is_src="false"
AC_ARG_WITH(apache,
[  --with-apache=DIR      Build static Apache module. DIR is the pathname 
                        to the Apache source directory.],
[
    if ${TEST} ! -z "$WEBSERVER" ; then
        AC_MSG_ERROR([Sorry cannot use --with-apxs=${APXS} and --with-apache=${withval} togother, please choose one of both])
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

           apache_dir=${withval}
           apache_dir_is_src="true"
           AC_MSG_RESULT(${apache_dir})
        
           AC_MSG_CHECKING(for Apache include directory)

           if ${TEST} -d "${withval}/src/include" ; then
               # read osdir from the existing apache.
               osdir=`${GREP} '^OSDIR=' ${withval}/src/Makefile.config | ${SED} -e 's:^OSDIR=.*/os:os:'`
               if ${TEST} -z "${osdir}" ; then
                   osdir=os/unix
               fi
               apache_include="-I${withval}/src/include \
                   -I${withval}/src/${osdir}"
               WEBSERVER="apache-1.3"
               AC_MSG_RESULT([${apache_include}, version 1.3])
           else
               AC_MSG_ERROR([Sorry Apache 1.2.x is no longer supported.])
           fi
        else
           if ${TEST} -d "${withval}/include" ; then
              # osdir for Apache20.
              WEBSERVER="apache-2.0"
              apache_dir=${withval}
              apache_dir_is_src="true"
              AC_MSG_RESULT(${apache_dir})
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
	AC_MSG_RESULT(no apache given)
])
AC_SUBST(apache_include)
APACHE_DIR=${apache_dir}
AC_SUBST(APACHE_DIR)
])

dnl
dnl check for EAPI (static link only).
dnl

AC_DEFUN(JTC_CHECK_EAPI,[

dnl CFLAGS for EAPI mod_ssl (Apache 1.3)
dnl it also allows the CFLAGS environment variable.
CFLAGS="${CFLAGS}"
AC_ARG_ENABLE(
EAPI,
[  --enable-EAPI           Enable EAPI support (mod_ssl, Apache 1.3)],
[
case "${enableval}" in
    y | Y | YES | yes | TRUE | true )
        CFLAGS="${CFLAGS} -DEAPI"
        AC_MSG_RESULT([...Enabling EAPI Support...])
        ;;
esac
])
AC_SUBST(CFLAGS)
])


dnl
dnl set flags for apxs.
dnl

AC_DEFUN(JTC_SET_APXS_FLAGS,[
dnl the APXSCFLAGS is given by apxs to the C compiler
dnl the APXSLDFLAGS is given to the linker (for APRVARS).
APXSLDFLAGS=""
APXSCFLAGS=""
if ${TEST} -n "${CFLAGS}" ; then
	APXSCFLAGS="${CFLAGS}"
fi
dnl the APXSLDFLAGS is normaly empty but APXSCFLAGS is not.
if ${TEST} -n "${LDFLAGS}" ; then
	APXSLDFLAGS="-Wl,${LDFLAGS}"
fi
AC_SUBST(APXSCFLAGS)
AC_SUBST(APXSLDFLAGS)
])
