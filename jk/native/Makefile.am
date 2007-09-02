# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
