# Gnu makefile is required
# use -D options to overrides defaults


#ifndef APACHE2_HOME
APACHE2_HOME=/opt/apache2
#endif

include ${APACHE2_HOME}/build/config_vars.mk
# Yes, we use the same properties as ant
include ../../../build.properties


# ---------- File list creation -------------------- 
# Same behavior as ant - 'all files from a dir'. 
# Excludes are not yet implemented.
JK_DIR := ../..
BUILD_DIR = ${JK_DIR}/../build/WEB-INF/jk2/apache2

COMMON_C_FILES := $(wildcard ${JK_DIR}/common/*.c )
A2_C_FILES := $(wildcard ${JK_DIR}/server/apache2/*.c )
H_FILES := $(wildcard ${JK_DIR}/include/*.h )

COMMON_LO_FILES := $(patsubst ${JK_DIR}/common/%, ${BUILD_DIR}/%, \
			 $(patsubst %c, %lo, ${COMMON_C_FILES} ))
A2_LO_FILES := $(patsubst ${JK_DIR}/server/apache2/%, ${BUILD_DIR}/%, \
			 $(patsubst %c, %lo, ${A2_C_FILES} ))


# ---------- Compile rules --------------------
JK_CFLAGS=-I${JK_DIR}/include -I${JAVA_HOME}/include  \
          ${EXTRA_CPPFLAGS} ${EXTRA_CFLAGS} \
	  -I${APACHE2_HOME}/include


.PHONY: all


VPATH=.:../../common

.c.lo:
	 ${LIBTOOL} --mode=compile ${CC} ${CFLAGS} -c $< -o $>

${BUILD_DIR}/%.lo: ${JK_DIR}/common/%.c
	 ${LIBTOOL} --mode=compile ${CC} ${CFLAGS} ${JK_CFLAGS} -c $< -o $@

${BUILD_DIR}/%.lo: ${JK_DIR}/server/apache2/%.c
	${LIBTOOL} --mode=compile ${CC} ${CFLAGS} ${JK_CFLAGS} -c $< -o $@


# ---------- Targets -------------------- 

all: prepare ${APACHE2_HOME}/modules/mod_jk.so

${APACHE2_HOME}/modules/mod_jk.so: ${BUILD_DIR}/libjk.la
	( cd ${BUILD_DIR} ; ${LIBTOOL} --mode=finish libjk.la  )
	( cd ${BUILD_DIR} ; ${LIBTOOL} --mode=install --debug cp libjk.la ${APACHE2_HOME}/modules )

${BUILD_DIR}/libjk.la: ${COMMON_LO_FILES} ${A2_LO_FILES}
	${LIBTOOL} --mode=link ${CC} $< -o $@

${COMMON_C_FILES} ${A2_C_FILES}: ${H_FILES}

prepare: 
	mkdir -p ${BUILD_DIR}

clean: 
	rm ${BUILD_DIR}/*.lo ${BUILD_DIR}/*.la ${BUILD_DIR}/*.o