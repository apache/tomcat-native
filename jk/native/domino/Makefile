# Makefile for Tomcat Domino connector

CC=gcc
LD=gcc -shared

CCFLAGS=-Wall -O3
LDFLAGS=-shared

#################################################################################
#										#
# These will almost certainly need to be changed to match the specifics of your	#
# Domino and Tomcat installations.						#
#										#
#################################################################################

# The root of your Domino installation. Mine's in /usr/local/lotus, but your's
# may well be /opt/lotus
NOTESROOT=/usr/local/lotus

# The place where the Notes API is installed
NOTESAPI=$(NOTESROOT)/notesapi

# The Domino program directory.
NOTESHOME=$(NOTESROOT)/notes/5061/linux

# The Domino data directory (the directory containing names.nsf)
NOTESDATA=$(NOTESROOT)/notes/data

# The include path for the Notes C API headers
NOTESINC=$(NOTESAPI)/include

# Where tomcat is installed. This is where conf, lib, webapps et al normally are
TOMCATHOME=/usr/local/apache/tomcat

# Your JDK's include directory
JAVAINC=$(JAVA_HOME)/include

#################################################################################
#										#
# You probably won't need to change anything below here.			#
#										#
#################################################################################

JK=../common
LIBS=-lc -ldl -lnotes -L$(NOTESHOME)

INC=-I$(NOTESINC) -I$(JAVAINC) -I$(JK)
DEFS=-DLINUX -DUNIX

JKOBJ = $(JK)/jk_ajp12_worker.o $(JK)/jk_ajp13.o $(JK)/jk_ajp13_worker.o \
	$(JK)/jk_connect.o $(JK)/jk_jni_worker.o $(JK)/jk_lb_worker.o \
	$(JK)/jk_map.o $(JK)/jk_msg_buff.o $(JK)/jk_nwmain.o \
	$(JK)/jk_pool.o $(JK)/jk_sockbuf.o $(JK)/jk_uri_worker_map.o \
	$(JK)/jk_util.o $(JK)/jk_worker.o $(JK)/jk_ajp14.o \
	$(JK)/jk_ajp14_worker.o $(JK)/jk_context.o $(JK)/jk_md5.o \
	$(JK)/jk_ajp_common.o

LOBJ  =	jk_dsapi_plugin.o inifile.o
OBJ   = $(JKOBJ) $(LOBJ)

TARG  = libtomcat.so
INI   = libtomcat.ini

all: $(TARG) $(INI) 

$(TARG): $(OBJ)
	$(LD) $(LDFLAGS) $(LIBS) $(OBJ) -o $(TARG)

$(INI): mkini.sh Makefile
	./mkini.sh $(TOMCATHOME) > $(INI)

.c.o:
	$(CC) $(CCFLAGS) $(DEFS) $(INC) -o $@ -c $< 

clean:
	rm -f $(OBJ)

install: $(TARG) $(INI)
	cp $(TARG) $(NOTESHOME)
	cp $(INI) $(NOTESDATA)

# Various dependencies
jk_dsapi_plugin.o: jk_dsapi_plugin.c config.h inifile.h
inifile.o: inifile.c config.h inifile.h
