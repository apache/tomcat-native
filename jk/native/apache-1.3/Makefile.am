## configure should make the Makefile out of this file.

APXS=@APXS@
OS=@OS@
JAVA_HOME=@JAVA_HOME@
CP=@CP@
APACHE_DIR=@APACHE_DIR@
MKDIR=@MKDIR@
DESTDIR=@APACHE_DIR@
libdir=/libexec

VPATH=.:../common
APACHE_FILES = Makefile.tmpl Makefile.libdir libjk.module

if MAKE_DYNAMIC_APACHE
JK=
else
JK=../common
endif
JK_INCL=-DUSE_APACHE_MD5 -I ${JK}
JAVA_INCL=-I ${JAVA_HOME}/include -I ${JAVA_HOME}/include/${OS}
JAVA_LIB=-L ${JAVA_HOME}/jre/lib/${ARCH} -L ${JAVA_HOME}/lib/${ARCH}/native_threads
AM_CFLAGS=@apache_include@ -I../common

include ../common/list.mk.in

if MAKE_DYNAMIC_APACHE

OEXT=.lo

lib_LTLIBRARIES = libjk.la
libjk_la_SOURCES = mod_jk.c
libjk_la_LIBADD = $(APACHE_OBJECTS)

all: mod_jk.so

mod_jk.la: mod_jk.lo $(APACHE_OBJECTS)
	$(LIBTOOL) --mode=link ${COMPILE} -o $@ -module -rpath ${libexecdir} -avoid-version mod_jk.lo $(APACHE_OBJECTS)

mod_jk.so: mod_jk.la
	$(LIBTOOL) --mode=install cp $< `pwd`/$@


else

OEXT=.o

noinst_LIBRARIES = libjk.a
libjk_a_SOURCES = mod_jk.c
libjk_a_LIBADD = $(APACHE_OBJECTS)

install-data-local:
	@echo ""
	@echo "Copying files to Apache Modules Directory..."
	-${MKDIR} ${APACHE_DIR}/src/modules/jk
	${CP} ${APACHE_FILES} ${APACHE_DIR}/src/modules/jk
	${CP} ${noinst_LIBRARIES} ${APACHE_DIR}/src/modules/jk
	@echo ""
	@echo "Please be sure to re-compile Apache..."
	@echo ""
	@echo "cd ${APACHE_DIR}"
	@echo "./config-status --activate-module=src/modules/jk/libjk.a"
	@echo "make"
	@echo ""
endif

clean:
	-rm *.o *.so *.a
