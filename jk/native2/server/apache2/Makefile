# Gnu makefile and libtool are required
# use -D options to overrides defaults

#ifndef APACHE2_HOME
APACHE2_HOME=/opt/apache2
#endif

JK_DIR := ../..
BUILD_DIR = ${JK_DIR}/../build/jk2/apache2


# Extract EXTRA_CFLAGS and EXTRA_CPPFLAGS - same flags used during apache2 
# compilation
include ${APACHE2_HOME}/build/config_vars.mk

# Yes, we use the same properties file as ant
include ../../../build.properties

LIBTOOL=${APACHE2_HOME}/build/libtool 

# It doesn't hurt if we include all
INCLUDES= -I${JK_DIR}/include \
          -I${APACHE2_HOME}/include \
          -I${JAVA_HOME}/include \
          -I${JAVA_HOME}/include\linux \
          -I${JAVA_HOME}/include\hp-ux 

JK_CFLAGS=-DCHUNK_SIZE=4096 -DUSE_APACHE_MD5 -DHAS_APR -DHAVE_JNI

###### Based on rules.mk ##########################################
ALL_CFLAGS   = $(EXTRA_CFLAGS) $(NOTEST_CFLAGS) $(CFLAGS)
ALL_CPPFLAGS = $(DEFS) $(EXTRA_CPPFLAGS) $(NOTEST_CPPFLAGS) $(CPPFLAGS)
ALL_LDFLAGS  = $(EXTRA_LDFLAGS) $(NOTEST_LDFLAGS) $(LDFLAGS)
ALL_LIBS     = $(EXTRA_LIBS) $(NOTEST_LIBS) $(LIBS)
ALL_INCLUDES = $(INCLUDES) $(EXTRA_INCLUDES)

# Compile commands
COMPILE      = $(CC)  $(ALL_CFLAGS) $(ALL_CPPFLAGS) $(ALL_INCLUDES)

SH_COMPILE = $(LIBTOOL) --mode=compile $(COMPILE) -prefer-pic 
MOD_LINK = $(LIBTOOL) --mode=link $(CC) -module -shared $(LT_LDFLAGS) $(ALL_LDFLAGS) 

#############################################################################

# ---------- File list creation -------------------- 
# Same behavior as ant - 'all files from a dir'. 
# Excludes are not yet implemented.

COMMON_C_FILES := $(wildcard ${JK_DIR}/common/*.c )
JNI_C_FILES := $(wildcard ${JK_DIR}/jni/*.c )
A2_C_FILES := $(wildcard ${JK_DIR}/server/apache2/*.c )
H_FILES := $(wildcard ${JK_DIR}/include/*.h )

COMMON_LO_FILES := $(patsubst ${JK_DIR}/common/%, ${BUILD_DIR}/%, \
			 $(patsubst %c, %lo, ${COMMON_C_FILES} ))
JNI_LO_FILES := $(patsubst ${JK_DIR}/jni/%, ${BUILD_DIR}/%, \
			 $(patsubst %c, %lo, ${JNI_C_FILES} ))
A2_LO_FILES := $(patsubst ${JK_DIR}/server/apache2/%, ${BUILD_DIR}/%, \
			 $(patsubst %c, %lo, ${A2_C_FILES} ))


# ---------- Compile rules --------------------

.PHONY: all


VPATH=.:../../common

.c.lo:
	 ${SH_COMPILE} -c $< -o $>

${BUILD_DIR}/%.lo: ${JK_DIR}/common/%.c
	 ${SH_COMPILE} -c $< -o $@

${BUILD_DIR}/%.lo: ${JK_DIR}/jni/%.c
	 ${SH_COMPILE} -c $< -o $@

${BUILD_DIR}/%.lo: ${JK_DIR}/server/apache2/%.c
	 ${SH_COMPILE} -c $< -o $@


# ---------- Targets -------------------- 

all: prepare ${BUILD_DIR}/mod_jk2.so ${BUILD_DIR}/jkjni.so

${BUILD_DIR}/jkjni.so: ${JNI_LO_FILES}
	$(MOD_LINK) -o $@ $^

${BUILD_DIR}/mod_jk2.so: ${COMMON_LO_FILES} ${A2_LO_FILES}
	${MOD_LINK} -o $@ $^ 

${COMMON_C_FILES} ${A2_C_FILES}: ${H_FILES}

prepare: 
	mkdir -p ${BUILD_DIR}

clean: 
	rm -rf ${BUILD_DIR}/*.lo ${BUILD_DIR}/*.la ${BUILD_DIR}/*.o ${BUILD_DIR}/.libs ${BUILD_DIR}/*.so