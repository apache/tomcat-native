#
# Tell automake what it should do
AUTOMAKE_OPTIONS = foreign
MAINTAINERCLEANFILES=config.cache config.status config.log \
Makefile.in configure

SUBDIRS = @WEBSERVER@

all:
	target="all"; \
	list='$(SUBDIRS)'; \
	for i in $$list; do \
	    echo "Making $$target in $$i"; \
	    if test "$$i" != "."; then \
	       (cd $$i && $(MAKE) $$target) || exit 1; \
	    fi; \
	done;

apidocs: common/*.h
	../../scandoc/scandoc.pl -i ../../scandoc/template.pl -p \
	./docs/api/ -dproject="mod_jk Library" common/*.h common/*.c
