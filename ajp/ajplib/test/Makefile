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
AJP_OBJECTS = httpd_wrap.lo testajp.lo

INCLUDES = -I $(INCLUDE) -I $(APR_INCLUDE) -I $(APU_INCLUDE)

include $(APR_DIR)/build/apr_rules.mk
include $(APR_DIR)/build/config_vars.mk

all: testajp

testajp: $(AJP_OBJECTS)
	$(LINK) $(AJP_OBJECTS) $(AP_LIBS)

clean:
	rm -f *.o *.so *.lo *.la *.slo
	rm -rf .libs
	rm -f testajp
