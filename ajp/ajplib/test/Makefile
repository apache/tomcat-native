#
# You need an installed httpd-2.x to use this Makefile
#
APACHE_DIR=/home2/apache20/apache20
APR_DIR=$(APACHE_DIR)
APU_DIR=$(APACHE_DIR)

# in httpd-2.x sources...
#APR_DIR=$(APACHE_DIR)/srclib/apr
#APU_DIR=$(APACHE_DIR)/srclib/apr-util

APR_INCLUDE=$(APR_DIR)/include
APU_INCLUDE=$(APU_DIR)/include

SRC_DIR=.
INCLUDE=.
AJP_OBJECTS = ajp_link.lo  ajp_msg.lo  httpd_wrap.lo
AJP_PROGRAM = testajp.lo
AJP_LIB     = lib_ajp.la

INCLUDES = -I $(INCLUDE) -I $(APR_INCLUDE) -I $(APU_INCLUDE)

include $(APR_DIR)/build/apr_rules.mk
include $(APR_DIR)/build/config_vars.mk

all: testajp

testajp: $(AJP_PROGRAM) $(AJP_LIB)
	$(LINK) $(AJP_PROGRAM) $(AJP_LIB) $(AP_LIBS)

$(AJP_LIB): $(AJP_OBJECTS)
	$(LINK) $(AJP_OBJECTS)

clean:
	rm -f *.o *.so *.lo *.la *.slo
	rm -rf .libs
	rm -f testajp
