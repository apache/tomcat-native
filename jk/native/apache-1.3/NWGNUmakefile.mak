#
# Makefile for mod_jk (NetWare version - gnu make)
# created by Guenter Knauf <fuankg@apache.org>
#

#
# Make sure all needed macro's are defined
#

# Edit the path below to point to the base of your NetWare Java SDK.
ifndef NW_JDK
NW_JDK	= d:/projects/sdks/java-nw
endif

JKCOMMON = ../common

#
# Get the 'head' of the build environment if necessary.  This includes default
# targets and paths to tools
#

ifndef EnvironmentDefined
include $(AP_WORK)\NWGNUhead.inc
endif

#
# These directories will be at the beginning of the include list, followed by
# INCDIRS
#
XINCDIRS	+= \
			$(JKCOMMON) \
			$(NW_JDK)/include \
			$(NW_JDK)/include/netware \
			$(SRC)\include \
			$(NWOS) \
			$(EOLIST)

#
# These flags will come after CFLAGS
#
XCFLAGS		+= \
			$(EOLIST)

#
# These defines will come after DEFINES
#
XDEFINES	+= \
			$(EOLIST)

#
# These flags will be added to the link.opt file
#
XLFLAGS		+= \
			$(EOLIST)

#
# These values will be appended to the correct variables based on the value of
# RELEASE
#
ifeq "$(RELEASE)" "debug"
XINCDIRS	+= \
			$(EOLIST)

XCFLAGS		+= \
			$(EOLIST)

XDEFINES	+= \
			$(EOLIST)

XLFLAGS		+= \
			$(EOLIST)
endif

ifeq "$(RELEASE)" "noopt"
XINCDIRS	+= \
			$(EOLIST)

XCFLAGS		+= \
			$(EOLIST)

XDEFINES	+= \
			$(EOLIST)

XLFLAGS		+= \
			$(EOLIST)
endif

ifeq "$(RELEASE)" "release"
XINCDIRS	+= \
			$(EOLIST)

XCFLAGS		+= \
			$(EOLIST)

XDEFINES	+= \
			$(EOLIST)

XLFLAGS		+= \
			$(EOLIST)
endif

#
# These are used by the link target if an NLM is being generated
# This is used by the link 'name' directive to name the nlm.  If left blank
# TARGET_nlm (see below) will be used.
#
NLM_NAME	= mod_jk

#
# This is used by the link '-desc ' directive. 
# If left blank, NLM_NAME will be used.
#
NLM_DESCRIPTION	= Apache $(AP_VERSION_STR) plugin for Jakarta/Tomcat $(JK_VERSION_STR)

#
# This is used by the '-threadname' directive.  If left blank,
# NLM_NAME Thread will be used.
#
NLM_THREAD_NAME	= JK Module

#
# If this is specified, it will override VERSION value in 
# $(AP_WORK)\NWGNUenvironment.inc
#
NLM_VERSION	= $(JK_VERSION)

#
# If this is specified, it will override the default of 64K
#
NLM_STACK_SIZE	= 49152

#
# If this is specified it will be used by the link '-entry' directive
#
NLM_ENTRY_SYM	= _lib_start
#NLM_ENTRY_SYM	= _lib_start_ws

#
# If this is specified it will be used by the link '-exit' directive
#
NLM_EXIT_SYM	= _lib_stop
#NLM_EXIT_SYM	= _lib_stop_ws

#
# If this is specified it will be used by the link '-flags' directive
#
NLM_FLAGS	=

#
# Declare all target files (you must add your files here)
#

#
# If there is an NLM target, put it here
#
TARGET_nlm = \
	$(OBJDIR)/$(NLM_NAME).nlm \
	$(EOLIST)

#
# If there is an LIB target, put it here
#
TARGET_lib = \
	$(EOLIST)

#
# These are the OBJ files needed to create the NLM target above.
# Paths must all use the '/' character
#
FILES_nlm_objs = \
	$(OBJDIR)/jk_nwmain.o \
	$(OBJDIR)/jk_ajp12_worker.o \
	$(OBJDIR)/jk_ajp13.o \
	$(OBJDIR)/jk_ajp13_worker.o \
	$(OBJDIR)/jk_ajp14.o \
	$(OBJDIR)/jk_ajp14_worker.o \
	$(OBJDIR)/jk_ajp_common.o \
	$(OBJDIR)/jk_connect.o \
	$(OBJDIR)/jk_context.o \
	$(OBJDIR)/jk_jni_worker.o \
	$(OBJDIR)/jk_lb_worker.o \
	$(OBJDIR)/jk_map.o \
	$(OBJDIR)/jk_md5.o \
	$(OBJDIR)/jk_msg_buff.o \
	$(OBJDIR)/jk_pool.o \
	$(OBJDIR)/jk_shm.o \
	$(OBJDIR)/jk_sockbuf.o \
	$(OBJDIR)/jk_status.o \
	$(OBJDIR)/jk_uri_worker_map.o \
	$(OBJDIR)/jk_util.o \
	$(OBJDIR)/jk_worker.o \
	$(OBJDIR)/$(NLM_NAME).o \
	$(EOLIST)

#
# These are the LIB files needed to create the NLM target above.
# These will be added as a library command in the link.opt file.
#
FILES_nlm_libs = \
	$(NWOS)/$(OBJDIR)/libpre.o \
	$(EOLIST)

#	$(NWOS)/$(OBJDIR)/libprews.o

#
# These are the modules that the above NLM target depends on to load.
# These will be added as a module command in the link.opt file.
#
FILES_nlm_modules = \
	$(EOLIST)

#
# If the nlm has a msg file, put it's path here
#
FILE_nlm_msg =
 
#
# If the nlm has a hlp file put it's path here
#
FILE_nlm_hlp =

#
# If this is specified, it will override $(NWOS)\copyright.txt.
#
#FILE_nlm_copyright = 'Copyright (c) 2000-2005 The Apache Software Foundation. All rights reserved.'
FILE_nlm_copyright =


#
# Any additional imports go here
#
FILES_nlm_Ximports = \
	@ApacheCore.imp \
	@threads.imp \
	@clib.imp \
	@nlmlib.imp \
	@socklib.imp \
	$(EOLIST)
 
#   
# Any symbols exported to here
#
FILES_nlm_exports = \
	jk_module \
	$(EOLIST)

#   
# These are the OBJ files needed to create the LIB target above.
# Paths must all use the '/' character
#
FILES_lib_objs = \
	$(EOLIST)

#
# implement targets and dependancies (leave this section alone)
#

libs :: $(OBJDIR) $(TARGET_lib)

nlms :: libs $(TARGET_nlm)

#
# Updated this target to create necessary directories and copy files to the 
# correct place.  (See $(AP_WORK)\NWGNUhead.inc for examples)
#
install :: nlms FORCE
	copy $(OBJDIR)\$(NLM_NAME).nlm $(INSTALL)\Apache\modules

#
# Any specialized rules here
#
vpath %.c . $(JKCOMMON)

$(OBJDIR)/version.inc: $(JKCOMMON)/jk_version.h $(SRC)/include/httpd.h $(OBJDIR)
	@echo Creating $@
	@awk -f ../../../common/build/get_ver.awk $<  $(SRC)/include/httpd.h > $@

# Include the version info retrieved from jk_version.h
-include $(OBJDIR)/version.inc

#
# Include the 'tail' makefile that has targets that depend on variables defined
# in this makefile
#

include $(AP_WORK)/NWGNUtail.inc

